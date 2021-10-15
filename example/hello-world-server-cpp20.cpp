// Copyright 2021 Dennis Hezel
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

#include "protos/helloworld.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <optional>
#include <thread>

int main()
{
    std::optional<std::thread> shutdown_thread;

    // begin-snippet: server-side-helloworld
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    helloworld::Greeter::AsyncService service;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    boost::asio::basic_signal_set signals{grpc_context, SIGINT, SIGTERM};
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            while (true)
            {
                grpc::ServerContext server_context;
                helloworld::HelloRequest request;
                grpc::ServerAsyncResponseWriter<helloworld::HelloReply> writer{&server_context};
                bool request_ok = co_await agrpc::request(&helloworld::Greeter::AsyncService::RequestSayHello, service,
                                                          server_context, request, writer);
                if (!request_ok)
                {
                    co_return;
                }
                helloworld::HelloReply response;
                response.set_message("Hello " + request.name());
                bool finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
            }
        },
        boost::asio::detached);
    // end-snippet

    signals.async_wait(
        [&](auto&&, auto&&)
        {
            // This will cause all coroutines to run to completion normally
            // while returning 'false' from RPC related steps
            shutdown_thread.emplace(
                [&]
                {
                    server->Shutdown();
                });

            // Or call 'grpc_context.stop()' here instead which causes all coroutines
            // to end at their next suspension point.
            // Then call 'server->Shutdown()' after the call to 'grpc_context.run()' returns.
        });

    grpc_context.run();
    shutdown_thread->join();
}