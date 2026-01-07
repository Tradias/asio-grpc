// Copyright 2026 Dennis Hezel
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

#include <thread>

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

void explicit_io_context_separate_threads()
{
    asio::io_context io_context{1};
    agrpc::GrpcContext grpc_context;

    /* [run_io_context_separate_thread] */
    std::thread grpc_context_thread{[&]
                                    {
                                        grpc_context.run();
                                    }};
    io_context.run();
    grpc_context_thread.join();
    /* [run_io_context_separate_thread] */
}

void explicit_io_context_samr_thread()
{
    asio::io_context io_context{1};
    agrpc::GrpcContext grpc_context;

    /* [agrpc_run_io_context_and_grpc_context] */
    // First, initiate the io_context's thread_local variables by posting on it. The io_context uses them to optimize
    // dynamic memory allocations. This is an optional step but it can improve performance.
    asio::post(io_context,
               [&]
               {
                   agrpc::run(grpc_context, io_context,
                              [&]
                              {
                                  return grpc_context.is_stopped();
                              });
               });
    io_context.run();
    /* [agrpc_run_io_context_and_grpc_context] */
}
