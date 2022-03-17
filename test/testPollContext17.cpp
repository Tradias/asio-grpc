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
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"
#include "utils/rpc.hpp"

#include <agrpc/pollContext.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0))
{
TEST_CASE_FIXTURE(test::GrpcContextTest, "PollContext")
{
    asio::io_context io_context;
    asio::steady_timer timer{io_context};
    grpc::Alarm alarm;
    std::size_t alarms{};
    std::size_t timers{};
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    while (alarms < 10)
                    {
                        agrpc::wait(alarm, test::hundred_milliseconds_from_now(), yield);
                        ++alarms;
                    }
                });
    asio::spawn(io_context,
                [&](asio::yield_context yield)
                {
                    while (timers < 10)
                    {
                        timer.expires_after(std::chrono::milliseconds(100));
                        timer.async_wait(yield);
                        ++timers;
                    }
                });
    agrpc::PollContext context{io_context.get_executor()};
    context.poll(grpc_context);
    const auto start = std::chrono::steady_clock::now();
    io_context.run();
    const auto end = std::chrono::steady_clock::now();
    CHECK_MESSAGE(std::chrono::milliseconds(1200) > end - start,
                  std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(););
    CHECK_EQ(10, alarms);
    CHECK_EQ(10, timers);
}
}