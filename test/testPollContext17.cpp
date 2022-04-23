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
#include "utils/manualLifetime.hpp"
#include "utils/time.hpp"

#include <agrpc/pollContext.hpp>
#include <doctest/doctest.h>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcContextTest, "PollContext can process asio::post")
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
    const auto create_io_context_work_guard = [&]
    {
        return asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked);
    };
    test::ManualLifetime<decltype(create_io_context_work_guard())> work;
    work.construct(create_io_context_work_guard());
    agrpc::PollContext poll_context{io_context.get_executor()};
    poll_context.async_poll(grpc_context,
                            [&](auto&&)
                            {
                                if (io_context.stopped())
                                {
                                    CHECK(invoked);
                                    work.construct(create_io_context_work_guard());
                                    return true;
                                }
                                CHECK_FALSE(invoked);
                                return false;
                            });
    asio::post(io_context,
               [&]
               {
                   asio::post(grpc_context,
                              [&, g = create_io_context_work_guard()]
                              {
                                  asio::post(io_context,
                                             [&]
                                             {
                                                 // Next grpc_context.poll() will reset the stopped state
                                                 CHECK(grpc_context.is_stopped());
                                                 asio::post(grpc_context,
                                                            [&, g = create_io_context_work_guard()]
                                                            {
                                                                invoked = true;
                                                            });
                                             });
                              });
               });
    work.destruct();
    work.destruct();
    io_context.run();
    CHECK(invoked);
}

struct MyIntrusiveTraits : agrpc::DefaultPollContextTraits
{
    static constexpr std::chrono::nanoseconds MAX_LATENCY{0};
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "PollContextTraits can specify zero max latency")
{
    bool invoked{};
    asio::io_context io_context;
    agrpc::PollContext<asio::any_io_executor, MyIntrusiveTraits> poll_context{io_context.get_executor()};
    poll_context.async_poll(grpc_context,
                            [count = 0](auto&) mutable
                            {
                                ++count;
                                return 15 == count;
                            });
    asio::post(grpc_context,
               [&]
               {
                   invoked = true;
               });
    io_context.run();
    CHECK(invoked);
}

struct MyTraits
{
};

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "PollContextTraits can use traits that do not inherit from DefaultPollContextTraits")
{
    bool invoked{};
    asio::io_context io_context;
    agrpc::PollContext<asio::any_io_executor, MyTraits> poll_context{io_context.get_executor()};
    poll_context.async_poll(grpc_context,
                            [count = 0](auto&) mutable
                            {
                                ++count;
                                return 15 == count;
                            });
    asio::post(grpc_context,
               [&]
               {
                   invoked = true;
               });
    io_context.run();
    CHECK(invoked);
}
}