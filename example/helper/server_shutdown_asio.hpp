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

#ifndef AGRPC_HELPER_SERVER_SHUTDOWN_ASIO_HPP
#define AGRPC_HELPER_SERVER_SHUTDOWN_ASIO_HPP

#include <agrpc/grpc_executor.hpp>
#include <boost/asio/signal_set.hpp>
#include <grpcpp/server.h>

#include <atomic>
#include <thread>

namespace example
{
// ---------------------------------------------------
// A helper to properly shut down a gRPC server without deadlocking.
// ---------------------------------------------------
struct ServerShutdown
{
    ServerShutdown(grpc::Server& server, agrpc::GrpcContext& grpc_context)
        : server_(server), signals_(grpc_context, SIGINT, SIGTERM)
    {
        signals_.async_wait(
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
        if (!is_shutdown_.exchange(true))
        {
            // This will cause all coroutines to run to completion normally
            // while returning `false` from rpc related steps. Also cancels the signals
            // so that the GrpcContext will eventually run out of work and return
            // from `run()`.
            // We cannot call server.Shutdown() on the same thread that runs a GrpcContext because that can lead to a
            // deadlock, therefore create a new thread.
            shutdown_thread_ = std::thread(
                [&]
                {
                    signals_.cancel();
                    // Shutdown will wait for all outstanding rpcs to complete normally. Alternatively use Shutdown with
                    // deadline to cancel them after a certain time. For Shutdown to ever complete some other thread
                    // must be calling grpc_context.run().
                    server_.Shutdown();
                });
        }
    }

    ~ServerShutdown()
    {
        if (shutdown_thread_.joinable())
        {
            shutdown_thread_.join();
        }
        else
        {
            server_.Shutdown();
        }
    }

    grpc::Server& server_;
    boost::asio::basic_signal_set<agrpc::GrpcExecutor> signals_;
    std::atomic_bool is_shutdown_{};
    std::thread shutdown_thread_;
};
}

#endif  // AGRPC_HELPER_SERVER_SHUTDOWN_ASIO_HPP
