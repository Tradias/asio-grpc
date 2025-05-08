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

#include "utils/grpc_client_server_callback_test.hpp"

#include "utils/grpc_client_server_test_impl.hpp"

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

template GrpcClientServerTestTemplate<test::CallbackService>::GrpcClientServerTestTemplate();

template GrpcClientServerTestTemplate<test::CallbackService>::~GrpcClientServerTestTemplate();
}  // namespace test
