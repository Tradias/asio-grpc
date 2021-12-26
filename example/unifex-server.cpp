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
#include <unifex/just.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

unifex::task<void> handle_unary_request(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
    example::v1::Request request;
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service, server_context, request,
                                 writer, agrpc::use_sender(grpc_context)))
    {
        co_return;
    }
    example::v1::Response response;
    response.set_integer(request.integer());
    co_await agrpc::finish(writer, response, grpc::Status::OK, agrpc::use_sender(grpc_context));
}

unifex::task<void> handle_server_streaming_request(example::v1::Example::AsyncService& service,
                                                   agrpc::GrpcContext& grpc_context)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncWriter<example::v1::Response> writer{&server_context};
    example::v1::Request request;
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestServerStreaming, service, server_context,
                                 request, writer, agrpc::use_sender(grpc_context)))
    {
        co_return;
    }
    for (google::protobuf::int32 i = 0; i < request.integer(); ++i)
    {
        example::v1::Response response;
        response.set_integer(i);
        co_await agrpc::write(writer, response, agrpc::use_sender(grpc_context));
    }
    co_await agrpc::finish(writer, grpc::Status::OK, agrpc::use_sender(grpc_context));
}

unifex::task<void> handle_slow_unary_request(example::v1::Example::AsyncService& service,
                                             agrpc::GrpcContext& grpc_context)
{
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestSlowUnary, service, server_context,
                                 request, writer, agrpc::use_sender(grpc_context)))
    {
        co_return;
    }

    grpc::Alarm alarm;
    co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::milliseconds(request.integer()),
                         agrpc::use_sender(grpc_context));

    example::v1::Response response;
    co_await agrpc::finish(writer, response, grpc::Status::OK, agrpc::use_sender(grpc_context));
}

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

    unifex::sync_wait(unifex::when_all(handle_unary_request(service, grpc_context),
                                       handle_server_streaming_request(service, grpc_context),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));

    server->Shutdown();
}