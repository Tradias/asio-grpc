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

#include "utils/asioUtils.hpp"
#include "utils/grpcContextTest.hpp"
#include "utils/time.hpp"

#include <agrpc/pollContext.hpp>
#include <doctest/doctest.h>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0))
{
TEST_CASE_FIXTURE(test::GrpcContextTest, "PollContext asio::post")
{
    const auto expected_thread = std::this_thread::get_id();
    bool invoked{false};
    asio::io_context io_context;
    agrpc::PollContext poll_context{io_context.get_executor()};
    grpc_context.work_started();
    asio::post(io_context,
               [&]()
               {
                   CHECK_EQ(std::this_thread::get_id(), expected_thread);
                   asio::post(grpc_context,
                              [&]
                              {
                                  CHECK_EQ(std::this_thread::get_id(), expected_thread);
                                  invoked = true;
                                  grpc_context.work_finished();
                              });
               });
    poll_context.poll(grpc_context);
    io_context.run();
    CHECK(invoked);
}
}