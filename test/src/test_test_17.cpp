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

#include "utils/client_rpc.hpp"
#include "utils/doctest.hpp"
#include "utils/test.hpp"

#include <agrpc/client_rpc.hpp>

TEST_CASE_FIXTURE(test::MockTest, "mock unary request")
{
    test::set_up_unary_test(*this);
    test::spawn_and_run(grpc_context,
                        [&](auto&& yield)
                        {
                            grpc::ClientContext client_context;
                            test::msg::Response response;
                            test::UnaryInterfaceClientRPC::request(grpc_context, stub, client_context, {}, response,
                                                                   yield);
                            CHECK_EQ(42, response.integer());
                        });
}

TEST_CASE_FIXTURE(test::MockTest, "mock server streaming request")
{
    test::set_up_server_streaming_test(*this);
    test::spawn_and_run(grpc_context,
                        [&](auto&& yield)
                        {
                            test::msg::Request request;
                            test::ServerStreamingInterfaceClientRPC rpc{grpc_context};
                            CHECK(rpc.start(stub, request, yield));
                            test::msg::Response response;
                            CHECK(rpc.read(response, yield));
                            CHECK_EQ(42, response.integer());
                        });
}