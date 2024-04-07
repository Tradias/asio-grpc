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
GrpcClientServerTest::GrpcClientServerTest()
    : stub(test::v1::Test::NewStub(channel)),
      server_context_lifetime(std::in_place),
      server_context(*server_context_lifetime)
{
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
}

GrpcClientServerTest::~GrpcClientServerTest()
{
    client_context_lifetime.reset();
    stub.reset();
    channel.reset();
    server_context_lifetime.reset();
    if (server)
    {
        server->Shutdown();
    }
    grpc_context_lifetime.reset();
    server.reset();
}
}  // namespace test
