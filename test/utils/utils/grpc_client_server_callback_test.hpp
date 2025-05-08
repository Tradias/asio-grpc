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

#ifndef AGRPC_UTILS_GRPC_CLIENT_SERVER_CALLBACK_TEST_HPP
#define AGRPC_UTILS_GRPC_CLIENT_SERVER_CALLBACK_TEST_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/grpc_client_server_test.hpp"

#include <grpcpp/server_context.h>

namespace test
{
struct CallbackService : test::v1::Test::CallbackService
{
    grpc::ServerUnaryReactor* Unary(grpc::CallbackServerContext* context, const test::msg::Request* request,
                                    test::msg::Response* response) override;

    grpc::ServerWriteReactor<::test::msg::Response>* ServerStreaming(grpc::CallbackServerContext* context,
                                                                     const test::msg::Request* request) override;

    grpc::ServerReadReactor<::test::msg::Request>* ClientStreaming(grpc::CallbackServerContext* context,
                                                                   test::msg::Response* response) override;

    grpc::ServerBidiReactor<::test::msg::Request, ::test::msg::Response>* BidirectionalStreaming(
        grpc::CallbackServerContext* context) override;

    std::function<grpc::ServerUnaryReactor*(grpc::CallbackServerContext*, const test::msg::Request*,
                                            test::msg::Response*)>
        unary;
    std::function<grpc::ServerWriteReactor<::test::msg::Response>*(grpc::CallbackServerContext*,
                                                                   const test::msg::Request*)>
        server_streaming;
    std::function<grpc::ServerReadReactor<::test::msg::Request>*(grpc::CallbackServerContext*, test::msg::Response*)>
        client_streaming;
    std::function<grpc::ServerBidiReactor<::test::msg::Request, ::test::msg::Response>*(grpc::CallbackServerContext*)>
        bidirectional_streaming;
};

using GrpcClientServerCallbackTest = GrpcClientServerTestTemplate<test::CallbackService>;
}  // namespace test

#endif  // AGRPC_UTILS_GRPC_CLIENT_SERVER_CALLBACK_TEST_HPP
