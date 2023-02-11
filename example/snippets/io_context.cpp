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

#include "example/v1/example.grpc.pb.h"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

namespace asio = boost::asio;

void implicit_io_context()
{
    /* [implicit_io_context] */
    agrpc::GrpcContext grpc_context;
    asio::signal_set signals{grpc_context, SIGINT, SIGTERM};
    signals.async_wait(
        [](const std::error_code&, int)
        {
            // executed in the thread that called grpc_context.run().
        });
    grpc_context.run();
    /* [implicit_io_context] */
}

void explicit_io_context()
{
    example::v1::Example::Stub stub{grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials())};
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address_v4("127.0.0.1"), 8000);

    /* [co_spawn_io_context_and_grpc_context] */
    asio::io_context io_context{1};

    agrpc::GrpcContext grpc_context;
    auto grpc_context_work_guard = asio::make_work_guard(grpc_context);

    asio::co_spawn(
        io_context,  // Spawning onto the io_context means that completed operations will switch back to the it before
                     // resuming the coroutine ...
        [&]() -> asio::awaitable<void>
        {
            asio::ip::tcp::socket socket(io_context);
            co_await socket.async_connect(endpoint, asio::use_awaitable);
            co_await asio::async_write(socket, asio::buffer("example"), asio::use_awaitable);

            using RPC = agrpc::RPC<&example::v1::Example::Stub::PrepareAsyncUnary>;
            grpc::ClientContext client_context;
            client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
            RPC::Request request;
            request.set_integer(42);
            RPC::Response response;
            // ... using bind_executor however, we can remain on the thread of the GrpcContext.
            co_await RPC::request(grpc_context, stub, client_context, request, response,
                                  asio::bind_executor(asio::system_executor{}, asio::use_awaitable));

            grpc_context_work_guard.reset();
        },
        asio::detached);
    /* [co_spawn_io_context_and_grpc_context] */

    /* [run_io_context_separate_thread] */
    std::thread io_context_thread{[&]
                                  {
                                      io_context.run();
                                  }};
    grpc_context.run();
    io_context_thread.join();
    /* [run_io_context_separate_thread] */

    /* [agrpc_run_io_context_shared_work] */
    // Assuming that the io_context is the "main" context and that some work has been submitted to it prior.
    // First, initiate the io_context's thread_local variables by posting on it. The io_context uses them to optimize
    // dynamic memory allocations.
    // Then undo the work counting of asio::post.
    // Run GrpcContext and io_context until both stop.
    // Finally, redo the work counting.
    asio::post(io_context,
               [&]
               {
                   io_context.get_executor().on_work_finished();
                   agrpc::run(grpc_context, io_context);
                   io_context.get_executor().on_work_started();
               });
    io_context.run();
    /* [agrpc_run_io_context_shared_work] */
}
