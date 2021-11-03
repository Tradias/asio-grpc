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

#ifndef AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP
#define AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP

#include "protos/test.grpc.pb.h"
#include "utils/grpcContextTest.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/server_context.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace test
{
struct GrpcClientServerTest : test::GrpcContextTest
{
    uint16_t port;
    std::string address;
    test::v1::Test::AsyncService service;
    std::unique_ptr<test::v1::Test::Stub> stub;
    grpc::ServerContext server_context;
    grpc::ClientContext client_context;

    GrpcClientServerTest();

    ~GrpcClientServerTest();
};
}  // namespace test

#endif  // AGRPC_UTILS_GRPCCLIENTSERVERTEST_HPP
