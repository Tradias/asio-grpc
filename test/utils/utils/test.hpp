// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_UTILS_TEST_HPP
#define AGRPC_UTILS_TEST_HPP

#include "test/v1/test_mock.grpc.pb.h"
#include "utils/grpc_context_test.hpp"

namespace test
{
struct MockTest : test::GrpcContextTest
{
    testing::NiceMock<test::v1::MockTestStub> stub;
};

struct MockClientAsyncResponseReader : grpc::ClientAsyncResponseReaderInterface<test::msg::Response>
{
    MOCK_METHOD0(StartCall, void());
    MOCK_METHOD1(ReadInitialMetadata, void(void*));
    MOCK_METHOD3(Finish, void(test::msg::Response*, grpc::Status*, void*));
};

struct MockClientAsyncReader : grpc::ClientAsyncReaderInterface<test::msg::Response>
{
    MOCK_METHOD1(StartCall, void(void*));
    MOCK_METHOD1(ReadInitialMetadata, void(void*));
    MOCK_METHOD2(Finish, void(grpc::Status*, void*));
    MOCK_METHOD2(Read, void(test::msg::Response*, void*));
};

std::unique_ptr<testing::NiceMock<MockClientAsyncResponseReader>> set_up_unary_test(MockTest& test);

void set_up_server_streaming_test(MockTest& test);
}  // namespace test

#endif  // AGRPC_UTILS_TEST_HPP
