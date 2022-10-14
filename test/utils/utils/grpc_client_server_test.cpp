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

#include "utils/grpc_client_server_test.hpp"

#include "test/v1/test.grpc.pb.h"

#include <boost/chrono.hpp>
#include <grpcpp/server_context.h>

#include <iostream>

namespace test
{
GrpcClientServerTest::GrpcClientServerTest()
    : server_context_lifetime(std::in_place), server_context(*server_context_lifetime)
{
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    stub = test::v1::Test::NewStub(this->channel);
}

GrpcClientServerTest::~GrpcClientServerTest()
{
    std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()" << std::endl;
    client_context_lifetime.reset();
    std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()::client_context_lifetime" << std::endl;
    stub.reset();
    std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()::stub" << std::endl;
    server_context_lifetime.reset();
    std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()::server_context_lifetime" << std::endl;
    if (server)
    {
        server->Shutdown();
        std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()::Shutdown" << std::endl;
    }
    grpc_context_lifetime.reset();
    std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()::grpc_context_lifetime" << std::endl;
    server.reset();
    std::cout << boost::chrono::steady_clock::now() << "~GrpcClientServerTest()::server" << std::endl;
}
}  // namespace test
