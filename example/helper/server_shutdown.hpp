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

#ifndef AGRPC_HELPER_SERVER_SHUTDOWN_HPP
#define AGRPC_HELPER_SERVER_SHUTDOWN_HPP

#include <agrpc/grpc_executor.hpp>
#include <boost/asio/signal_set.hpp>
#include <grpcpp/server.h>

#include <atomic>

namespace example
{
// ---------------------------------------------------
// A helper to properly shut down a gRPC without deadlocking.
// ---------------------------------------------------
struct ServerShutdown
{
    grpc::Server& server;
    boost::asio::basic_signal_set<agrpc::GrpcContext::executor_type> signals;
    std::atomic_bool is_shutdown{};
    std::thread shutdown_thread;

    ServerShutdown(grpc::Server& server, agrpc::GrpcContext& grpc_context)
        : server(server), signals(grpc_context, SIGINT, SIGTERM)
    {
        signals.async_wait(
            [&](auto&& ec, auto&&)
            {
                if (boost::asio::error::operation_aborted != ec)
                {
                    shutdown();
                }
            });
    }

    void shutdown()
    {
        if (!is_shutdown.exchange(true))
        {
            // This will cause all coroutines to run to completion normally
            // while returning `false` from RPC related steps. Also cancel the signals
            // so that the GrpcContext will eventually run out of work and return
            // from `run()`.
            // We cannot call server.Shutdown() on the same thread that runs a GrpcContext because that could lead to
            // deadlock, therefore create a new thread.
            shutdown_thread = std::thread(
                [&]
                {
                    signals.cancel();
                    server.Shutdown();
                });
            // Alternatively call `grpc_context.stop()` here instead which causes all coroutines
            // to end at their next suspension point.
            // Then call `server->Shutdown()` after the call to `grpc_context.run()` returns
            // or `.reset()` the grpc_context and go into another `grpc_context.run()`
        }
    }

    ~ServerShutdown()
    {
        if (shutdown_thread.joinable())
        {
            shutdown_thread.join();
        }
        else if (!is_shutdown.exchange(true))
        {
            server.Shutdown();
        }
    }
};
}

#endif  // AGRPC_HELPER_SERVER_SHUTDOWN_HPP
