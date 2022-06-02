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
#include "utils/doctest.hpp"
#include "utils/grpcContextTest.hpp"
#include "utils/manualLifetime.hpp"
#include "utils/time.hpp"

#include <agrpc/pollContext.hpp>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
struct PollContextTest : test::GrpcContextTest
{
    asio::io_context io_context;

    auto create_io_context_work_guard()
    {
        return asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked);
    }
};

TEST_CASE_FIXTURE(PollContextTest, "PollContext can process asio::post")
{
    const auto expected_thread = std::this_thread::get_id();
    bool invoked{false};
    std::optional guard{create_io_context_work_guard()};
    asio::post(io_context,
               [&]
               {
                   CHECK_EQ(std::this_thread::get_id(), expected_thread);
                   asio::post(grpc_context,
                              [&]
                              {
                                  CHECK_EQ(std::this_thread::get_id(), expected_thread);
                                  invoked = true;
                                  guard.reset();
                              });
               });
    agrpc::run<>(grpc_context, io_context);
    CHECK(invoked);
}

TEST_CASE_FIXTURE(PollContextTest,
                  "PollContext.async_poll custom stop predicate that ends when io_context runs out of work")
{
    bool invoked{false};
    asio::post(io_context,
               [&, g = get_work_tracking_executor()]
               {
                   asio::post(grpc_context,
                              [&, g = create_io_context_work_guard()]
                              {
                                  asio::post(io_context,
                                             [&, g = get_work_tracking_executor()]
                                             {
                                                 CHECK_FALSE(grpc_context.is_stopped());
                                                 asio::post(grpc_context,
                                                            [&, g = create_io_context_work_guard()]
                                                            {
                                                                invoked = true;
                                                            });
                                             });
                              });
               });
    agrpc::run<>(grpc_context, io_context,
                 [&](auto&&)
                 {
                     if (io_context.stopped())
                     {
                         CHECK(invoked);
                         return true;
                     }
                     CHECK_FALSE(invoked);
                     return false;
                 });
    CHECK(invoked);
}

struct MyIntrusiveTraits : agrpc::DefaultPollContextTraits
{
    static constexpr std::chrono::nanoseconds MAX_LATENCY{0};
};

TEST_CASE_FIXTURE(PollContextTest, "PollContextTraits can specify zero max latency")
{
    bool invoked{};
    asio::post(grpc_context,
               [&]
               {
                   invoked = true;
               });
    agrpc::run<MyIntrusiveTraits>(grpc_context, io_context,
                                  [count = 0](auto&) mutable
                                  {
                                      ++count;
                                      return 15 == count;
                                  });
    CHECK(invoked);
}

struct MyTraits
{
};

TEST_CASE_FIXTURE(PollContextTest, "PollContextTraits can use traits that do not inherit from DefaultPollContextTraits")
{
    int invoked_count{};
    agrpc::run<MyTraits>(grpc_context, io_context,
                         [&, count = 0](auto&) mutable
                         {
                             if (count % 4 == 0 || count % 4 - 1 == 0)
                             {
                                 asio::post(io_context,
                                            [&]
                                            {
                                                ++invoked_count;
                                            });
                             }
                             ++count;
                             return 10 == count;
                         });
    CHECK_EQ(4, invoked_count);
    CHECK(io_context.poll());
    CHECK_EQ(5, invoked_count);
}

struct Counter
{
    int value{};
};

struct MyCustomPoll
{
    static bool poll(Counter& counter)
    {
        ++counter.value;
        return true;
    }
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "PollContextTraits can use traits to customize polling")
{
    int invoked_count_grpc_context{};
    Counter counter{};
    const auto guard = get_work_tracking_executor();
    agrpc::run<MyCustomPoll>(grpc_context, counter,
                             [&, count = 0](auto&) mutable
                             {
                                 if (count % 6 == 0)
                                 {
                                     asio::post(grpc_context,
                                                [&]
                                                {
                                                    ++invoked_count_grpc_context;
                                                });
                                 }
                                 CHECK_EQ(count, counter.value);
                                 ++count;
                                 return 25 == count;
                             });
    CHECK_EQ(4, invoked_count_grpc_context);
    CHECK_EQ(24, counter.value);
}
}