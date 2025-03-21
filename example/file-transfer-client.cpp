// Copyright 2024 Dennis Hezel
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
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"
#include "scope_guard.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/stream_file.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

namespace asio = boost::asio;

// begin-snippet: client-side-file-transfer

// Example showing how to transfer files over a streaming RPC. Stack buffers are used to customize memory allocation.

// end-snippet

// The use of `asio::awaitable<bool, agrpc::GrpcExecutor>` is not required but `agrpc::GrpcExecutor` is slightly smaller
// and faster to copy than the `asio::any_io_executor` of the default `asio::awaitable`.
asio::awaitable<bool, agrpc::GrpcExecutor> make_double_buffered_send_file_request(agrpc::GrpcContext& grpc_context,
                                                                                  asio::io_context& io_context,
                                                                                  example::v1::ExampleExt::Stub& stub,
                                                                                  const std::string& file_path)
{
    using RPC = agrpc::ClientRPC<&example::v1::ExampleExt::Stub::PrepareAsyncSendFile>;

    // Use a larger chunk size in production code, like 64'0000.
    // Here we use a smaller value so that our payload needs more than one chunk.
    static constexpr std::size_t CHUNK_SIZE = 5;

    // These buffers are used to customize allocation of completion handlers.
    example::Buffer<250> buffer1;
    example::Buffer<64> buffer2;

    RPC rpc{grpc_context};
    rpc.context().set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    google::protobuf::Empty response;
    if (!co_await rpc.start(stub, response, buffer1.bind_allocator()))
    {
        co_return false;
    }

    // Switch to the io_context and open the file there to avoid blocking the GrpcContext.
    co_await asio::post(buffer1.bind_allocator(asio::bind_executor(io_context)));

    // Relying on CTAD here to create a `asio::basic_stream_file<asio::io_context::executor_type>` which is slightly
    // more performant than the default `asio::stream_file` that is templated on `asio::any_io_executor`.
    //
    // If you get exception: io_uring_queue_init: Cannot allocate memory [system:12]
    // then run `ulimit -l 65535`, see also https://github.com/axboe/liburing/issues/157
    asio::basic_stream_file file{io_context.get_executor(), file_path, asio::stream_file::read_only};

    example::v1::SendFileRequest first_read_buffer;
    first_read_buffer.mutable_content()->resize(CHUNK_SIZE);

    // `asio::bind_executor` prevents context switching to this_coro::executor (the GrpcContext in our case) when
    // async_read_some completes. We do not need to switch because rpc::write is thread-safe.
    auto bytes_read =
        co_await file.async_read_some(asio::buffer(*first_read_buffer.mutable_content()),
                                      buffer1.bind_allocator(asio::bind_executor(asio::system_executor{})));

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

        auto [completion_order, ec, next_bytes_read, ok] =
            co_await asio::experimental::make_parallel_group(
                file.async_read_some(asio::buffer(*next->mutable_content()),
                                     buffer1.bind_allocator(asio::bind_executor(asio::system_executor{}))),
                rpc.write(*current, buffer2.bind_allocator()))
                .async_wait(asio::experimental::wait_for_all(), buffer1.bind_allocator());
        if (!ok)
        {
            // Lost connection to server, no reason to finish this RPC.
            co_return false;
        }
        is_eof = asio::error::eof == ec;
        bytes_read = next_bytes_read;

        std::swap(current, next);
    }

    file.close();

    // Signal that we are done sending chunks
    current->mutable_content()->resize(bytes_read);
    co_await rpc.write(*current, grpc::WriteOptions{}.set_last_message(), buffer1.bind_allocator());

    co_return (co_await rpc.finish(buffer1.bind_allocator())).ok();
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
    const auto host = std::string("localhost:") + port;

    example::v1::ExampleExt::Stub stub_ext{grpc::CreateChannel(host, grpc::InsecureChannelCredentials())};
    agrpc::GrpcContext grpc_context;

    asio::io_context io_context{1};
    std::optional guard{asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked)};

    try
    {
        // Create the file to be send
        const auto temp_dir = argc >= 3 ? std::filesystem::path{argv[2]} : std::filesystem::temp_directory_path();
        const auto file_path = (temp_dir / "file-transfer-input.txt").string();
        {
            std::ofstream stream{file_path, std::ofstream::trunc};
            stream.exceptions(std::ofstream::failbit);
            stream << "content";
        }

        asio::co_spawn(
            grpc_context,
            [&]() -> asio::awaitable<void, agrpc::GrpcExecutor>
            {
                abort_if_not(
                    co_await make_double_buffered_send_file_request(grpc_context, io_context, stub_ext, file_path));
            },
            example::RethrowFirstArg{});

        std::thread io_context_thread{&run_io_context, std::ref(io_context)};
        example::ScopeGuard on_exit{[&]
                                    {
                                        guard.reset();
                                        io_context_thread.join();
                                    }};
        grpc_context.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}