// Copyright 2025 Dennis Hezel
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

#include "utils/test.hpp"

#include <agrpc/test.hpp>

namespace test
{
void set_up_unary_test(MockTest& test)
{
    auto mock_reader = std::make_unique<testing::NiceMock<MockClientAsyncResponseReader>>();
    EXPECT_CALL(*mock_reader, Finish)
        .WillOnce(
            [&](test::msg::Response* response, grpc::Status*, void* tag)
            {
                response->set_integer(42);
                agrpc::process_grpc_tag(test.grpc_context, tag, true);
            });
    EXPECT_CALL(test.stub, PrepareAsyncUnaryRaw)
        .WillOnce(
            [reader = std::make_shared<decltype(mock_reader)>(std::move(mock_reader))](auto&&...)
            {
                // GRPC wraps the return value into a unique_ptr with a specialized std::default_delete that does
                // nothing
                return reader->get();
            });
}

void set_up_server_streaming_test(MockTest& test)
{
    EXPECT_CALL(test.stub, PrepareAsyncServerStreamingRaw)
        .WillOnce(
            [&](auto&&...)
            {
                auto mock_reader = std::make_unique<testing::NiceMock<MockClientAsyncReader>>();
                EXPECT_CALL(*mock_reader, Read)
                    .WillOnce(
                        [&](test::msg::Response* response, void* tag)
                        {
                            response->set_integer(42);
                            agrpc::process_grpc_tag(test.grpc_context, tag, true);
                        });
                EXPECT_CALL(*mock_reader, StartCall)
                    .WillOnce(
                        [&](void* tag)
                        {
                            agrpc::process_grpc_tag(test.grpc_context, tag, true);
                        });
                // GRPC wraps the return value into a unique_ptr
                return mock_reader.release();
            });
}
}  // namespace test
