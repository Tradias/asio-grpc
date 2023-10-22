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

#include "grpc/health/v1/health.grpc.pb.h"
#include "helloworld/helloworld.grpc.pb.h"
#include "server_shutdown_asio.hpp"

#include <agrpc/asio_grpc.hpp>
#include <agrpc/health_check_service.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;

// begin-snippet: server-side-multi-threaded
// ---------------------------------------------------
// Multi-threaded server performing 20 unary requests
// ---------------------------------------------------
// end-snippet

void register_request_handler(agrpc::GrpcContext& grpc_context, helloworld::Greeter::AsyncService& service,
                              example::ServerShutdown& shutdown)
{
    agrpc::repeatedly_request(
        &helloworld::Greeter::AsyncService::RequestSayHello, service,
        asio::bind_executor(
            grpc_context,
            [&](grpc::ServerContext&, helloworld::HelloRequest& request,
                grpc::ServerAsyncResponseWriter<helloworld::HelloReply>& writer) -> asio::awaitable<void>
            {
                helloworld::HelloReply response;
                response.set_message("Hello " + request.name());
                co_await agrpc::finish(writer, response, grpc::Status::OK);

                // In this example we shut down the server after 20 requests
                static std::atomic_int counter{};
                if (19 == counter.fetch_add(1))
                {
                    shutdown.shutdown();
                }
            }));
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;
    const auto thread_count = std::thread::hardware_concurrency();

    helloworld::Greeter::AsyncService service;
    std::unique_ptr<grpc::Server> server;
    std::vector<std::unique_ptr<agrpc::GrpcContext>> grpc_contexts;

    {
        grpc::ServerBuilder builder;
        for (size_t i = 0; i < thread_count; ++i)
        {
            grpc_contexts.emplace_back(std::make_unique<agrpc::GrpcContext>(builder.AddCompletionQueue()));
        }
        builder.AddListeningPort(host, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        agrpc::add_health_check_service(builder);
        server = builder.BuildAndStart();
        agrpc::start_health_check_service(*server, *grpc_contexts.front());
    }

    example::ServerShutdown shutdown{*server, *grpc_contexts.front()};

    // Create one thread per GrpcContext.
    std::vector<std::thread> threads;
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(
            [&, i]
            {
                auto& grpc_context = *grpc_contexts[i];
                register_request_handler(grpc_context, service, shutdown);
                grpc_context.run();
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
}