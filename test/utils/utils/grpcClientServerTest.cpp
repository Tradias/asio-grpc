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

#include "utils/grpcClientServerTest.hpp"

#include "test/v1/test.grpc.pb.h"
#include "utils/freePort.hpp"
#include "utils/grpcContextTest.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/server_context.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace test
{
GrpcClientServerTest::GrpcClientServerTest()
    : port(test::get_free_port()), address(std::string{"0.0.0.0:"} + std::to_string(port))
{
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    stub = test::v1::Test::NewStub(
        grpc::CreateChannel(std::string{"localhost:"} + std::to_string(port), grpc::InsecureChannelCredentials()));
    client_context.set_deadline(test::five_seconds_from_now());
}

GrpcClientServerTest::~GrpcClientServerTest()
{
    stub.reset();
    server->Shutdown();
}
}  // namespace test
