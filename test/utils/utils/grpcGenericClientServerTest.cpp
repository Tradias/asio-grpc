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

#include "utils/grpcGenericClientServerTest.hpp"

#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/generic/generic_stub.h>

namespace test
{
GrpcGenericClientServerTest::GrpcGenericClientServerTest() : stub{this->channel}
{
    builder.RegisterAsyncGenericService(&service);
    this->server = builder.BuildAndStart();
}

// GrpcClientServerTest::~GrpcClientServerTest() { stub.reset(); }
}  // namespace test
