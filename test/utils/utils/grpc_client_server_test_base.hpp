// Copyright 2022 Dennis Hezel
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

#ifndef AGRPC_UTILS_GRPC_CLIENT_SERVER_TEST_BASE_HPP
#define AGRPC_UTILS_GRPC_CLIENT_SERVER_TEST_BASE_HPP

#include "utils/grpc_context_test.hpp"

#include <grpcpp/client_context.h>

#include <cstdint>
#include <optional>
#include <string>

namespace test
{
struct GrpcClientServerTestBase : virtual test::GrpcContextTest
{
    uint16_t port;
    std::string address;
    std::shared_ptr<grpc::Channel> channel;
    std::optional<grpc::ClientContext> client_context_lifetime;
    grpc::ClientContext& client_context;

    GrpcClientServerTestBase();

    ~GrpcClientServerTestBase();
};
}  // namespace test

#endif  // AGRPC_UTILS_GRPC_CLIENT_SERVER_TEST_BASE_HPP
