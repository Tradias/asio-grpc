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

#include "utils/grpc_client_server_test.hpp"

#include "test/v1/test.grpc.pb.h"

#include <grpcpp/server_context.h>

namespace test
{
grpc::ServerUnaryReactor* CallbackService::Unary(grpc::CallbackServerContext* context,
                                                 const test::msg::Request* request, test::msg::Response* response)
{
    if (unary)
    {
        return unary(context, request, response);
    }
    return nullptr;
}

grpc::ServerWriteReactor<::test::msg::Response>* CallbackService::ServerStreaming(grpc::CallbackServerContext* context,
                                                                                  const test::msg::Request* request)
{
    if (server_streaming)
    {
        return server_streaming(context, request);
    }
    return nullptr;
}

grpc::ServerReadReactor<::test::msg::Request>* CallbackService::ClientStreaming(grpc::CallbackServerContext* context,
                                                                                test::msg::Response* response)
{
    if (client_streaming)
    {
        return client_streaming(context, response);
    }
    return nullptr;
}

grpc::ServerBidiReactor<::test::msg::Request, ::test::msg::Response>* CallbackService::BidirectionalStreaming(
    grpc::CallbackServerContext* context)
{
    if (bidirectional_streaming)
    {
        return bidirectional_streaming(context);
    }
    return nullptr;
}

template <class Service>
GrpcClientServerTestTemplate<Service>::GrpcClientServerTestTemplate()
    : stub(test::v1::Test::NewStub(channel)),
      server_context_lifetime(std::in_place),
      server_context(*server_context_lifetime)
{
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
}

template GrpcClientServerTestTemplate<test::v1::Test::AsyncService>::GrpcClientServerTestTemplate();
template GrpcClientServerTestTemplate<test::CallbackService>::GrpcClientServerTestTemplate();

template <class Service>
GrpcClientServerTestTemplate<Service>::~GrpcClientServerTestTemplate()
{
    client_context_lifetime.reset();
    stub.reset();
    channel.reset();
    server_context_lifetime.reset();
    if (server)
    {
        server->Shutdown(std::chrono::system_clock::now() + std::chrono::milliseconds(10));
    }
    grpc_context_lifetime.reset();
    server.reset();
}

template GrpcClientServerTestTemplate<test::v1::Test::AsyncService>::~GrpcClientServerTestTemplate();
template GrpcClientServerTestTemplate<test::CallbackService>::~GrpcClientServerTestTemplate();
}  // namespace test
