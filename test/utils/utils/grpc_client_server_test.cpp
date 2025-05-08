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

#include "utils/grpc_client_server_test_impl.hpp"

namespace test
{
template GrpcClientServerTestTemplate<test::v1::Test::AsyncService>::GrpcClientServerTestTemplate();

template GrpcClientServerTestTemplate<test::v1::Test::AsyncService>::~GrpcClientServerTestTemplate();
}  // namespace test
