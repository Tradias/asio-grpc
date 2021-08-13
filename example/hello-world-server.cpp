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
#include <boost/asio/spawn.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

static void spawn(agrpc::GrpcContext& grpc_context, helloworld::Greeter::AsyncService& service)
{
    boost::asio::spawn(grpc_context,
                       [&](auto yield)
                       {
                           while (true)
                           {
                               grpc::ServerContext server_context;
                               helloworld::HelloRequest request;
                               grpc::ServerAsyncResponseWriter<helloworld::HelloReply> writer{&server_context};
                               bool request_ok = agrpc::request(&helloworld::Greeter::AsyncService::RequestSayHello,
                                                                service, server_context, request, writer, yield);
                               helloworld::HelloReply response;
                               std::string prefix("Hello ");
                               response.set_message(prefix + request.name());
                               bool finish_ok = agrpc::finish(writer, response, grpc::Status::OK, yield);
                           }
                       });
}

int main()
{
    grpc::ServerBuilder builder;
    std::unique_ptr<grpc::Server> server;
    helloworld::Greeter::AsyncService service;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    agrpc::GrpcContext grpc_context2{builder.AddCompletionQueue()};
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    auto guard = boost::asio::make_work_guard(grpc_context);
    auto guard2 = boost::asio::make_work_guard(grpc_context2);

    spawn(grpc_context, service);
    spawn(grpc_context2, service);

    std::thread t{[&]
                  {
                      grpc_context2.run();
                  }};
    grpc_context.run();
    server->Shutdown();
    t.join();
}