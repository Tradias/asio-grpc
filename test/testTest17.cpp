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
#include "utils/asioUtils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpcContextTest.hpp"

#include <agrpc/rpc.hpp>
#include <agrpc/test.hpp>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
template <class Response>
struct MockAsyncWriter : grpc::ClientAsyncResponseReaderInterface<Response>
{
    MOCK_METHOD0(StartCall, void());
    MOCK_METHOD1(ReadInitialMetadata, void(void*));
    MOCK_METHOD3(Finish, void(Response* msg, grpc::Status* status, void*));
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "mock unary request")
{
    testing::NiceMock<test::v1::MockTestStub> mock_stub;
    testing::NiceMock<MockAsyncWriter<test::msg::Response>> mock_writer;
    EXPECT_CALL(mock_writer, Finish)
        .WillOnce(
            [&](test::msg::Response* response, grpc::Status*, void* tag)
            {
                response->set_integer(42);
                agrpc::process_grpc_tag(tag, true, grpc_context);
            });
    EXPECT_CALL(mock_stub, AsyncUnaryRaw).WillOnce(testing::Return(&mock_writer));
    test::spawn_and_run(grpc_context,
                        [&](auto&& yield)
                        {
                            grpc::ClientContext client_context;
                            test::msg::Request request;
                            const auto writer = agrpc::request(&test::v1::Test::StubInterface::AsyncUnary, mock_stub,
                                                               client_context, request, grpc_context);
                            grpc::Status status;
                            test::msg::Response response;
                            agrpc::finish(writer, response, status, yield);
                            CHECK_EQ(42, response.integer());
                        });
}
}