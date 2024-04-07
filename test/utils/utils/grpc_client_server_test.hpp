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

#ifndef AGRPC_UTILS_GRPC_CLIENT_SERVER_TEST_HPP
#define AGRPC_UTILS_GRPC_CLIENT_SERVER_TEST_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/grpc_client_server_test_base.hpp"

#include <grpcpp/server_context.h>

namespace test
{
struct GrpcClientServerTest : test::GrpcClientServerTestBase
{
    test::v1::Test::AsyncService service;
    std::unique_ptr<test::v1::Test::Stub> stub;
    std::optional<grpc::ServerContext> server_context_lifetime;
    grpc::ServerContext& server_context;

    GrpcClientServerTest();

    ~GrpcClientServerTest();
};
}  // namespace test

#endif  // AGRPC_UTILS_GRPC_CLIENT_SERVER_TEST_HPP
