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

#include "helloworld/helloworld.grpc.pb.h"
#include "rethrow_first_arg.hpp"

#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

namespace asio = boost::asio;

// begin-snippet: server-side-helloworld

// Server-side hello world which handles exactly one request from the client before shutting down.

// end-snippet
int main(int argc, const char** argv)
{
    const char* port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    helloworld::Greeter::AsyncService service;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    using RPC = agrpc::ServerRPC<&helloworld::Greeter::AsyncService::RequestSayHello>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [&](RPC& rpc, helloworld::HelloRequest& request) -> asio::awaitable<void>
        {
            helloworld::HelloReply response;
            response.set_message("Hello " + request.name());
            co_await rpc.finish(response, grpc::Status::OK);
            server->Shutdown();
        },
        example::RethrowFirstArg{});

    grpc_context.run();
}