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
#include "example/v1/exampleExt.grpc.pb.h"
#include "helper.hpp"
#include "whenBoth.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/stream_file.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <filesystem>
#include <fstream>

namespace asio = boost::asio;

// Example showing how to transfer files over a streaming RPC. Only a fixed number of dynamic memory allocations are
// performed. The use of `agrpc::GrpcAwaitable<bool>` is not required but `agrpc::GrpcExecutor` is slightly smaller and
// faster to copy than the `asio::any_io_executor` of the default `asio::awaitable`.
agrpc::GrpcAwaitable<bool> make_double_buffered_send_file_request(agrpc::GrpcContext& grpc_context,
                                                                  asio::io_context& io_context,
                                                                  example::v1::ExampleExt::Stub& stub,
                                                                  const std::string& file_path)
{
    // Use a larger chunk size in production code, like 64'0000
    static constexpr std::size_t CHUNK_SIZE = 5;

    // These buffers are used to customize allocation of completion handlers.
    example::Buffer<250> buffer1;
    example::Buffer<64> buffer2;

    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    google::protobuf::Empty response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::SendFileRequest>> writer;
    if (!co_await agrpc::request(&example::v1::ExampleExt::Stub::AsyncSendFile, stub, client_context, writer, response,
                                 buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE)))
    {
        co_return false;
    }

    // Switch to the io_context and open the file there to avoid blocking the GrpcContext.
    co_await asio::dispatch(buffer1.bind_allocator(asio::bind_executor(io_context, agrpc::GRPC_USE_AWAITABLE)));

    // Relying on CTAD here to create a `asio::basic_stream_file<asio::io_context::executor_type>` which is slightly
    // more performant than the default `asio::stream_file` that is templated on `asio::any_io_executor`.
    std::cout << "Create file: " << file_path << std::endl;
    asio::basic_stream_file file{io_context.get_executor()};
    std::cout << "Created file: " << file_path << std::endl;
    boost::system::error_code ec;
    file.open(file_path, asio::stream_file::read_only, ec);
    if (ec)
    {
        std::cout << "Error opened file: " << ec.message() << std::endl;
    }
    std::cout << "Opened file: " << file_path << std::endl;

    example::v1::SendFileRequest first_read_buffer;
    first_read_buffer.mutable_content()->resize(CHUNK_SIZE);

    // bind_executor prevents context switching to this_coro::executor when async_read_some completes.
    // We do not need to switch because agrpc::write is thread-safe.
    auto bytes_read = co_await file.async_read_some(
        asio::buffer(*first_read_buffer.mutable_content()),
        buffer1.bind_allocator(asio::bind_executor(io_context, agrpc::GRPC_USE_AWAITABLE)));
    std::cout << "Read bytes: " << bytes_read << std::endl;

    bool is_eof{false};

    example::v1::SendFileRequest second_read_buffer;

    auto current = &first_read_buffer;
    auto next = &second_read_buffer;
    while (!is_eof)
    {
        // We send 'current' to the server so make sure its size matches the number of bytes that were read.
        current->mutable_content()->resize(bytes_read);

        // Prepare for the next read from the file.
        next->mutable_content()->resize(CHUNK_SIZE);

        auto [ec_and_bytes_read, ok] =
            co_await example::when_both<std::pair<boost::system::error_code, std::size_t>, bool>(
                [&](auto&& completion_handler)
                {
                    // Again using bind_executor to avoid switching contexts in case this function completes after
                    // agrpc::write.
                    file.async_read_some(
                        asio::buffer(*next->mutable_content()),
                        buffer1.bind_allocator(asio::bind_executor(io_context, std::move(completion_handler))));
                },
                [&](auto&& completion_handler)
                {
                    // Need to bind_executor here because completion_handler is a simple invocable without an associated
                    // executor.
                    std::cout << "Sending: " << current->content() << std::endl;
                    agrpc::write(
                        *writer, *current,
                        buffer2.bind_allocator(asio::bind_executor(grpc_context, std::move(completion_handler))));
                });
        if (!ok)
        {
            // Lost connection to server, no reason to finish this RPC.
            co_return false;
        }
        is_eof = asio::error::eof == ec_and_bytes_read.first;
        bytes_read = ec_and_bytes_read.second;

        std::swap(current, next);
    }

    file.close();

    // Signal that we are done sending chunks
    current->mutable_content()->resize(bytes_read);
    current->set_finish_write(true);
    std::cout << "Sending done" << std::endl;
    co_await agrpc::write_last(*writer, *current, {}, buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE));

    grpc::Status status;
    co_await agrpc::finish(*writer, status, buffer1.bind_allocator(agrpc::GRPC_USE_AWAITABLE));

    co_return status.ok();
}

void run_io_context(asio::io_context& io_context)
{
    try
    {
        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception while running io_context: " << e.what() << std::endl;
        std::abort();
    }
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + port;

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto stub_ext = example::v1::ExampleExt::NewStub(channel);
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};

    asio::io_context io_context{1};
    auto guard = asio::make_work_guard(io_context);

    try
    {
        // Create the file to be send
        const auto temp_dir = argc >= 3 ? std::filesystem::path{argv[2]} : std::filesystem::temp_directory_path();
        const auto file_path = (temp_dir / "file-transfer-input.txt").string();
        std::filesystem::remove(file_path);
        {
            std::ofstream file{file_path};
            file << "content";
        }

        asio::co_spawn(
            grpc_context,
            [&]() -> agrpc::GrpcAwaitable<void>
            {
                abort_if_not(
                    co_await make_double_buffered_send_file_request(grpc_context, io_context, *stub_ext, file_path));
            },
            asio::detached);

        std::thread io_context_thread{&run_io_context, std::ref(io_context)};
        std::cout << "Start running" << std::endl;
        grpc_context.run();
        std::cout << "Running done" << std::endl;

        guard.reset();
        io_context_thread.join();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}