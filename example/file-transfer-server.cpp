// Copyright 2023 Dennis Hezel
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
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"
#include "scope_guard.hpp"
#include "server_shutdown_asio.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/write.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

namespace asio = boost::asio;

// begin-snippet: server-side-file-transfer
// ---------------------------------------------------
// Example showing how to transfer files over a streaming RPC. Stack buffers are used to customize memory allocation.
// ---------------------------------------------------
// end-snippet

using RPC = agrpc::ServerRPC<&example::v1::ExampleExt::AsyncService::RequestSendFile>;

inline constexpr asio::use_awaitable_t<agrpc::GrpcExecutor> USE_AWAITABLE{};

// The use of ` asio::awaitable<bool, agrpc::GrpcExecutor>` is not required but `agrpc::GrpcExecutor` is slightly
// smaller and faster to copy than the `asio::any_io_executor` of the default `asio::awaitable`.
asio::awaitable<bool, agrpc::GrpcExecutor> handle_send_file_request(asio::io_context& io_context, RPC& rpc,
                                                                    const std::string& file_path)
{
    // These buffers are used to customize allocation of completion handlers.
    example::Buffer<300> buffer1;
    example::Buffer<40> buffer2;

    example::v1::SendFileRequest first_buffer;

    // Read the first chunk from the client
    bool ok = co_await rpc.read(first_buffer, buffer1.bind_allocator(USE_AWAITABLE));

    if (!ok)
    {
        // Client hung up
        co_await rpc.finish({}, grpc::Status::OK, buffer1.bind_allocator(USE_AWAITABLE));
        co_return false;
    }

    example::v1::SendFileRequest second_buffer;

    // Switch to the io_context and open the file there to avoid blocking the GrpcContext.
    co_await asio::post(buffer1.bind_allocator(asio::bind_executor(io_context, USE_AWAITABLE)));

    // Relying on CTAD here to create a `asio::basic_stream_file<asio::io_context::executor_type>` which is slightly
    // more performant than the default `asio::stream_file` that is templated on `asio::any_io_executor`.
    //
    // If you get exception: io_uring_queue_init: Cannot allocate memory [system:12]
    // here then run `ulimit -l 65535`, see also https://github.com/axboe/liburing/issues/157
    asio::basic_stream_file file{io_context.get_executor(), file_path,
                                 asio::stream_file::write_only | asio::stream_file::create};

    // Lambda that writes the first argument to the file and simultaneously waits for the next message from the client.
    const auto write_and_read =
        [&](const example::v1::SendFileRequest& message_to_write, example::v1::SendFileRequest& message_to_read)
    {
        return asio::experimental::make_parallel_group(
                   [&](auto&& token)
                   {
                       // Using bind_executor to avoid switching contexts in case this function completes after
                       // agrpc::read.
                       return asio::async_write(
                           file, asio::buffer(message_to_write.content()),
                           buffer1.bind_allocator(asio::bind_executor(asio::system_executor{}, std::move(token))));
                   },
                   [&](auto&& token)
                   {
                       // Need to bind_executor here because `token` is a simple invocable without an
                       // associated executor.
                       return rpc.read(message_to_read, buffer2.bind_allocator(std::move(token)));
                   })
            .async_wait(asio::experimental::wait_for_all(), buffer1.bind_allocator(USE_AWAITABLE));
    };

    auto* current = &first_buffer;
    auto* next = &second_buffer;
    while (ok)
    {
        auto [completion_order, ec, bytes_written, next_ok] = co_await write_and_read(*current, *next);
        if (ec)
        {
            throw boost::system::system_error(ec);
        }
        ok = next_ok;
        std::swap(current, next);
    }

    co_return co_await rpc.finish({}, grpc::Status::OK, buffer1.bind_allocator(USE_AWAITABLE));
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

    example::v1::ExampleExt::AsyncService service_ext;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_ext);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});
    example::ServerShutdown shutdown{*server, grpc_context};

    asio::io_context io_context{1};
    std::optional guard{asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked)};

    try
    {
        // Prepare output file
        const auto temp_dir = argc >= 3 ? std::filesystem::path{argv[2]} : std::filesystem::temp_directory_path();
        const auto file_path = (temp_dir / "file-transfer-output.txt").string();
        std::filesystem::remove(file_path);

        agrpc::register_awaitable_rpc_handler<RPC>(
            grpc_context, service_ext,
            [&](RPC& rpc) -> asio::awaitable<void, agrpc::GrpcExecutor>
            {
                abort_if_not(co_await handle_send_file_request(io_context, rpc, file_path));
                shutdown.shutdown();
            },
            example::RethrowFirstArg{});

        std::thread io_context_thread{&run_io_context, std::ref(io_context)};
        example::ScopeGuard on_exit{[&]
                                    {
                                        guard.reset();
                                        io_context_thread.join();
                                    }};
        grpc_context.run();

        // Check that output file has the expected content
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
        return 1;
    }
    return 0;
}