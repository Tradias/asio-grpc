// Copyright 2026 Dennis Hezel
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

#include "utils/grpc_client_server_test_base.hpp"

#include "utils/free_port.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/time.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <cstdint>
#include <string>

namespace test
{
GrpcClientServerTestBase::GrpcClientServerTestBase()
    : port(test::get_free_port()),
      address(std::string{"0.0.0.0:"} + std::to_string(port)),
      client_context_lifetime(std::in_place),
      client_context(*client_context_lifetime)
{
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    channel = grpc::CreateChannel(std::string{"127.0.0.1:"} + std::to_string(port), grpc::InsecureChannelCredentials());
    client_context.set_deadline(test::five_seconds_from_now());
}

GrpcClientServerTestBase::~GrpcClientServerTestBase()
{
    client_context_lifetime.reset();
    channel.reset();
    if (server)
    {
        server->Shutdown();
    }
    grpc_context_lifetime.reset();
}
}  // namespace test
