// Copyright 2022 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "buffer.hpp"
#include "example/v1/example.grpc.pb.h"
#include "example/v1/exampleExt.grpc.pb.h"
#include "helper.hpp"
#include "whenBoth.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/write.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <filesystem>
#include <fstream>

namespace asio = boost::asio;

// Example showing how to transfer files over a streaming RPC. Only a fixed number of dynamic memory allocations are
// performed. The use of `agrpc::GrpcAwaitable<bool>` is not required but `agrpc::GrpcExecutor` is slightly smaller and
// faster to copy than the `asio::any_io_executor` of the default `asio::awaitable`.
agrpc::GrpcAwaitable<bool> handle_send_file_request(agrpc::GrpcContext& grpc_context, asio::io_context& io_context,
                                                    example::v1::ExampleExt::AsyncService& service,
                                                    const std::string& file_path)
{
    // These buffers are used to customize allocation of completion handlers.
    example::Buffer<320> buffer1;
    example::Buffer<64> buffer2;

    grpc::ServerContext server_context;
    grpc::ServerAsyncReader<google::protobuf::Empty, example::v1::SendFileRequest> responder{&server_context};
    if (!co_await agrpc::request(&example::v1::ExampleExt::AsyncService::RequestSendFile, service, server_context,
                                 responder, buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE)))
    {
        // Server is shutting down.
        co_return false;
    }

    example::v1::SendFileRequest first_write_buffer;

    // Read the first chunk from the client
    bool ok = co_await agrpc::read(responder, first_write_buffer, buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE));

    bool finish_write = first_write_buffer.finish_write();
    if (!ok && !finish_write)
    {
        // Client hang up or forgot to set finish_write.
        co_await agrpc::finish(responder, {}, grpc::Status::OK, buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE));
        co_return false;
    }

    example::v1::SendFileRequest second_write_buffer;

    // Switch to the io_context and open the file there to avoid blocking the GrpcContext.
    co_await asio::dispatch(buffer1.bind_allocator(asio::bind_executor(io_context, agrpc::GRPC_USE_AWAITABLE)));

    // Relying on CTAD here to create a `asio::basic_stream_file<asio::io_context::executor_type>` which is slightly
    // more performant than the default `asio::stream_file` that is templated on `asio::any_io_executor`.
    asio::basic_stream_file file{io_context.get_executor(), file_path,
                                 asio::stream_file::write_only | asio::stream_file::create};

    // Lambda that writes the first argument to the file and simultaneously waits for the next message from the client.
    const auto write_and_read = [&](const example::v1::SendFileRequest& message_to_write,
                                    example::v1::SendFileRequest& message_to_read)
        -> agrpc::GrpcAwaitable<std::pair<std::pair<boost::system::error_code, std::size_t>, bool>>
    {
        co_return co_await example::when_both<std::pair<boost::system::error_code, std::size_t>, bool>(
            [&](auto&& completion_handler)
            {
                if (message_to_write.content().empty())
                {
                    std::move(completion_handler)(std::pair<boost::system::error_code, std::size_t>{});
                }
                else
                {
                    // Using bind_executor to avoid switching contexts in case this function completes after
                    // agrpc::read.
                    asio::async_write(
                        file, asio::buffer(message_to_write.content()), asio::transfer_all(),
                        buffer1.bind_allocator(asio::bind_executor(io_context, std::move(completion_handler))));
                }
            },
            [&](auto&& completion_handler)
            {
                if (finish_write)
                {
                    std::move(completion_handler)(true);
                }
                else
                {
                    // Need to bind_executor here because completion_handler is a simple invocable without an associated
                    // executor.
                    agrpc::read(
                        responder, message_to_read,
                        buffer2.bind_allocator(asio::bind_executor(grpc_context, std::move(completion_handler))));
                }
            });
    };

    ok = (co_await write_and_read(first_write_buffer, second_write_buffer)).second;

    auto current = &second_write_buffer;  // second buffer is set to current
    auto next = &first_write_buffer;
    while (ok && !finish_write)
    {
        finish_write = current->finish_write();
        ok = (co_await write_and_read(*current, *next)).second;
        std::swap(current, next);
    }

    file.close();

    if (!ok && !finish_write)
    {
        // Client hang up or forgot to set finish_write.
        std::filesystem::remove(file_path);
    }

    co_return co_await agrpc::finish(responder, {}, grpc::Status::OK,
                                     buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE));
}

void run_io_context(asio::io_context& io_context)
{
    try
    {
        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception from io_context: " << e.what() << std::endl;
        std::abort();
    }
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    example::v1::ExampleExt::AsyncService service_ext;
    builder.RegisterService(&service_ext);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    asio::io_context io_context{1};
    auto guard = asio::make_work_guard(io_context);

    try
    {
        // Prepare output file
        const auto temp_dir = argc >= 3 ? std::filesystem::path{argv[2]} : std::filesystem::temp_directory_path();
        const auto file_path = (temp_dir / "file-transfer-output.txt").string();
        std::filesystem::remove(file_path);

        asio::co_spawn(
            grpc_context,
            [&]() -> agrpc::GrpcAwaitable<void>
            {
                abort_if_not(co_await handle_send_file_request(grpc_context, io_context, service_ext, file_path));
            },
            [](auto&& ep)
            {
                if (ep)
                {
                    std::rethrow_exception(ep);
                }
            });

        std::thread io_context_thread{&run_io_context, std::ref(io_context)};
        grpc_context.run();
        guard.reset();
        io_context_thread.join();

        // Check that output file has expected content
        std::string content;
        {
            std::ifstream stream{file_path};
            stream.exceptions(std::ifstream::failbit);
            stream >> content;
        }
        abort_if_not("content" == content);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        server->Shutdown();
        return 1;
    }

    server->Shutdown();
    return 0;
}