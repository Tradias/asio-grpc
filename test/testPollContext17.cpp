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

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcContextTest, "PollContext asio::post")
{
    const auto expected_thread = std::this_thread::get_id();
    bool invoked{false};
    asio::io_context io_context;
    agrpc::PollContext poll_context{io_context.get_executor()};
    grpc_context.work_started();
    asio::post(io_context,
               [&]
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
    poll_context.async_poll(grpc_context);
    io_context.run();
    CHECK(invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "PollContext.async_poll custom stop predicate that ends when io_context runs out of work")
{
    bool invoked{false};
    asio::io_context io_context;
    agrpc::PollContext poll_context{io_context.get_executor()};
    poll_context.async_poll(grpc_context,
                            [&](auto&&)
                            {
                                if (io_context.stopped())
                                {
                                    CHECK(invoked);
                                    io_context.get_executor().on_work_started();
                                    return true;
                                }
                                CHECK_FALSE(invoked);
                                return false;
                            });
    asio::post(io_context,
               [&]
               {
                   asio::post(grpc_context,
                              [&, g = asio::make_work_guard(io_context)]
                              {
                                  asio::post(io_context,
                                             [&]
                                             {
                                                 // Next grpc_context.poll() will reset the stopped state
                                                 CHECK(grpc_context.is_stopped());
                                                 asio::post(grpc_context,
                                                            [&, g = asio::make_work_guard(io_context)]
                                                            {
                                                                invoked = true;
                                                            });
                                             });
                              });
               });
    io_context.get_executor().on_work_finished();
    io_context.run();
    CHECK(invoked);
}
}