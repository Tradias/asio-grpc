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

#include "utils/freePort.hpp"
#include "utils/grpcClientServerTestBase.hpp"
#include "utils/grpcContextTest.hpp"
#include "utils/time.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <cstdint>
#include <string>

namespace test
{
GrpcClientServerTestBase::GrpcClientServerTestBase()
    : port(test::get_free_port()), address(std::string{"0.0.0.0:"} + std::to_string(port))
{
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    channel = grpc::CreateChannel(std::string{"localhost:"} + std::to_string(port), grpc::InsecureChannelCredentials());
    client_context.set_deadline(test::five_seconds_from_now());
}

GrpcClientServerTestBase::~GrpcClientServerTestBase() { server->Shutdown(); }
}  // namespace test
