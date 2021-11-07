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

#include "helper.hpp"
#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/when_all.hpp>

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    example::v1::Example::AsyncService service;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    unifex::sync_wait(unifex::when_all(
        [&]() -> unifex::task<void>
        {
            grpc::ServerContext server_context;
            grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
            example::v1::Request request;
            bool request_ok =
                co_await agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service, server_context,
                                        request, writer, agrpc::use_scheduler(grpc_context));
            if (!request_ok)
            {
                co_return;
            }
            example::v1::Response response;
            response.set_integer(request.integer());
            co_await agrpc::finish(writer, response, grpc::Status::OK, agrpc::use_scheduler(grpc_context));
        }(),
        [&]() -> unifex::task<void>
        {
            grpc_context.run();
            co_return;
        }()));

    server->Shutdown();
}