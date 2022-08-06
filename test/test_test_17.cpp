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

#include "test/v1/test_mock.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_context_test.hpp"

#include <agrpc/rpc.hpp>
#include <agrpc/test.hpp>

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

TEST_CASE_FIXTURE(MockTest, "mock unary request")
{
    testing::NiceMock<MockClientAsyncResponseReader> mock_reader;
    EXPECT_CALL(mock_reader, Finish)
        .WillOnce(
            [&](test::msg::Response* response, grpc::Status*, void* tag)
            {
                response->set_integer(42);
                agrpc::process_grpc_tag(grpc_context, tag, true);
            });
    EXPECT_CALL(stub, AsyncUnaryRaw).WillOnce(testing::Return(&mock_reader));
    test::spawn_and_run(grpc_context,
                        [&](auto&& yield)
                        {
                            grpc::ClientContext client_context;
                            test::msg::Request request;
                            const auto writer = agrpc::request(&test::v1::Test::StubInterface::AsyncUnary, stub,
                                                               client_context, request, grpc_context);
                            grpc::Status status;
                            test::msg::Response response;
                            agrpc::finish(writer, response, status, yield);
                            CHECK_EQ(42, response.integer());
                        });
}

struct MockClientAsyncReader : grpc::ClientAsyncReaderInterface<test::msg::Response>
{
    MOCK_METHOD1(StartCall, void(void*));
    MOCK_METHOD1(ReadInitialMetadata, void(void*));
    MOCK_METHOD2(Finish, void(grpc::Status*, void*));
    MOCK_METHOD2(Read, void(test::msg::Response*, void*));
};

TEST_CASE_FIXTURE(MockTest, "mock server streaming request")
{
    auto mock_reader = std::make_unique<testing::NiceMock<MockClientAsyncReader>>();
    EXPECT_CALL(*mock_reader, Read)
        .WillOnce(
            [&](test::msg::Response* response, void* tag)
            {
                response->set_integer(42);
                agrpc::process_grpc_tag(grpc_context, tag, true);
            });
    EXPECT_CALL(*mock_reader, StartCall)
        .WillOnce(
            [&](void* tag)
            {
                agrpc::process_grpc_tag(grpc_context, tag, true);
            });
    EXPECT_CALL(stub, PrepareAsyncServerStreamingRaw).WillOnce(testing::Return(mock_reader.release()));
    test::spawn_and_run(grpc_context,
                        [&](auto&& yield)
                        {
                            grpc::ClientContext client_context;
                            test::msg::Request request;
                            const auto [writer, ok] =
                                agrpc::request(&test::v1::Test::StubInterface::PrepareAsyncServerStreaming, stub,
                                               client_context, request, yield);
                            test::msg::Response response;
                            agrpc::read(writer, response, yield);
                            CHECK_EQ(42, response.integer());
                        });
}