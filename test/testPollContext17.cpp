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
TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.poll()")
{
    asio::io_context io_context;
    asio::steady_timer timer{io_context};
    grpc::Alarm alarm;
    bool wait_done{};
    asio::spawn(io_context,
                [&](asio::yield_context yield)
                {
                    asio::post(io_context,
                               [&]()
                               {
                                   grpc_context.poll();
                                   CHECK_FALSE(wait_done);
                                   timer.expires_after(std::chrono::milliseconds(110));
                                   timer.async_wait(
                                       [&](auto&&)
                                       {
                                           grpc_context.poll();
                                           CHECK(wait_done);
                                       });
                               });
                    agrpc::wait(alarm, test::hundred_milliseconds_from_now(),
                                asio::bind_executor(grpc_context,
                                                    [&](bool)
                                                    {
                                                        wait_done = true;
                                                    }));
                });
    io_context.run();
}
}