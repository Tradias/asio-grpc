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

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/high_level_client.hpp"
#include "utils/protobuf.hpp"
#include "utils/time.hpp"

#include <agrpc/high_level_client.hpp>

// struct UnifexTest : virtual test::GrpcContextTest
// {
//     template <class... Sender>
//     void run(Sender&&... sender)
//     {
//         grpc_context.work_started();
//         unifex::sync_wait(unifex::when_all(unifex::finally(unifex::when_all(std::forward<Sender>(sender)...),
//                                                            unifex::then(unifex::just(),
//                                                                         [&]
//                                                                         {
//                                                                             grpc_context.work_finished();
//                                                                         })),
//                                            unifex::then(unifex::just(),
//                                                         [&]
//                                                         {
//                                                             grpc_context.run();
//                                                         })));
//     }
// };

// struct UnifexHighLevelTest : test::HighLevelClientTest<test::BidirectionalStreamingRPC>, UnifexTest
// {
// };

// TEST_CASE_FIXTURE(UnifexHighLevelTest, "BidirectionalStreamingRPC success")
// {
//     run(
//         [&]() -> unifex::task<void>
//         {
//             CHECK(co_await test_server.request_rpc(use_sender()));
//             test_server.response.set_integer(1);
//             CHECK(co_await agrpc::read(test_server.responder, test_server.request, use_sender()));
//             CHECK_FALSE(co_await agrpc::read(test_server.responder, test_server.request, use_sender()));
//             CHECK_EQ(42, test_server.request.integer());
//             CHECK(co_await agrpc::write(test_server.responder, test_server.response, use_sender()));
//             CHECK(co_await agrpc::finish(test_server.responder, grpc::Status::OK, use_sender()));
//         },
//         [&]() -> unifex::task<void>
//         {
//             auto rpc = co_await request_rpc(agrpc::use_sender);
//             request.set_integer(42);
//             CHECK(co_await rpc.write(request, agrpc::use_sender));
//             CHECK(co_await rpc.writes_done(agrpc::use_sender));
//             CHECK(co_await rpc.read(response, agrpc::use_sender));
//             CHECK_EQ(1, response.integer());
//             CHECK(co_await rpc.writes_done(agrpc::use_sender));
//             CHECK_FALSE(co_await rpc.read(response, agrpc::use_sender));
//             CHECK_EQ(1, response.integer());
//             CHECK(co_await rpc.finish(agrpc::use_sender));
//             CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
//             CHECK(co_await rpc.finish(agrpc::use_sender));
//             CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
//         });
// }
