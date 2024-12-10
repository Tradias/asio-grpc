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

#include "helloworld/helloworld.grpc.pb.h"
#include "rethrow_first_arg.hpp"
#include "server_shutdown_asio.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/detached.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;

// begin-snippet: server-side-multi-threaded-alternative

// Multi-threaded server handling unary requests using callback API and single GrpcContext

// end-snippet

void register_request_handler(agrpc::GrpcContext& grpc_context, helloworld::Greeter::AsyncService& service,
                              example::ServerShutdown& shutdown)
{
    using RPC = agrpc::ServerRPC<&helloworld::Greeter::AsyncService::RequestSayHello>;
    agrpc::register_callback_rpc_handler<RPC>(
        grpc_context, service,
        [&](RPC::Ptr ptr, helloworld::HelloRequest& request)
        {
            helloworld::HelloReply response;
            response.set_message("Hello " + request.name());
            auto& rpc = *ptr;
            rpc.finish(response, grpc::Status::OK,
                       [&, p = std::move(ptr)](bool)
                       {
                           // In this example we shut down the server after 20 requests
                           static std::atomic_int counter{};
                           if (19 == counter.fetch_add(1))
                           {
                               shutdown.shutdown();
                           }
                       });
        },
        example::RethrowFirstArg{});
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;
    const auto thread_count = std::thread::hardware_concurrency();

    helloworld::Greeter::AsyncService service;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context(builder.AddCompletionQueue(), thread_count);
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    example::ServerShutdown shutdown{*server, grpc_context};

    // Create one thread per GrpcContext.
    std::vector<std::thread> threads;
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(
            [&]
            {
                register_request_handler(grpc_context, service, shutdown);
                grpc_context.run();
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
}