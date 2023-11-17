// Copyright 2023 Dennis Hezel
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

#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/time.hpp"

#include <agrpc/run.hpp>
#include <agrpc/wait.hpp>

#include <optional>
#include <thread>

struct RunTest : test::GrpcContextTest
{
    asio::io_context io_context;

    auto create_io_context_work_guard()
    {
        return asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked);
    }
};

TEST_CASE_FIXTURE(RunTest, "agrpc::run can process asio::post")
{
    const auto expected_thread = std::this_thread::get_id();
    bool invoked{false};
    std::optional guard{create_io_context_work_guard()};
    asio::post(io_context,
               [&]
               {
                   CHECK_EQ(std::this_thread::get_id(), expected_thread);
                   test::post(grpc_context,
                              [&]
                              {
                                  CHECK_EQ(std::this_thread::get_id(), expected_thread);
                                  invoked = true;
                                  guard.reset();
                              });
               });
    agrpc::run(grpc_context, io_context);
    CHECK(invoked);
}

TEST_CASE_FIXTURE(RunTest, "agrpc::run Custom stop predicate that ends when io_context runs out of work")
{
    bool invoked{false};
    asio::post(io_context,
               [&, g = get_work_tracking_executor()]
               {
                   test::post(grpc_context,
                              [&, g = create_io_context_work_guard()]
                              {
                                  asio::post(io_context,
                                             [&, g = get_work_tracking_executor()]
                                             {
                                                 CHECK_FALSE(grpc_context.is_stopped());
                                                 test::post(grpc_context,
                                                            [&, g = create_io_context_work_guard()]
                                                            {
                                                                invoked = true;
                                                            });
                                             });
                              });
               });
    agrpc::run(grpc_context, io_context,
               [&]()
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

struct MyIntrusiveTraits : agrpc::DefaultRunTraits
{
    static constexpr std::chrono::nanoseconds MAX_LATENCY{0};
};

TEST_CASE_FIXTURE(RunTest, "agrpc::run Traits can specify zero max latency")
{
    bool invoked{};
    test::post(grpc_context,
               [&]
               {
                   invoked = true;
               });
    agrpc::run<MyIntrusiveTraits>(grpc_context, io_context,
                                  [count = 0]() mutable
                                  {
                                      ++count;
                                      return 15 == count;
                                  });
    CHECK(invoked);
}

struct MyTraits
{
};

TEST_CASE_FIXTURE(RunTest, "agrpc::run Traits can use traits that do not inherit from DefaultRunTraits")
{
    int invoked_count{};
    const auto guard = create_io_context_work_guard();
    agrpc::run<MyTraits>(grpc_context, io_context,
                         [&, count = 0]() mutable
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
    CHECK_EQ(5, invoked_count);
    CHECK(io_context.poll());
    CHECK_EQ(6, invoked_count);
}

struct Counter
{
    int value{};

    static bool stopped() { return false; }
};

struct MyCustomPoll
{
    static bool poll(Counter& counter)
    {
        ++counter.value;
        return false;
    }

    template <class Rep, class Period>
    static bool run_for(Counter& counter, std::chrono::duration<Rep, Period>)
    {
        ++counter.value;
        return false;
    }
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::run Traits can use traits to customize polling")
{
    int invoked_count_grpc_context{};
    Counter counter{};
    const auto guard = get_work_tracking_executor();
    agrpc::run<MyCustomPoll>(grpc_context, counter,
                             [&, count = 0]() mutable
                             {
                                 if (count % 6 == 0)
                                 {
                                     test::post(grpc_context,
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

struct MyWaitTraits
{
    static constexpr std::chrono::seconds MAX_LATENCY{1};

    static bool poll(Counter&) { return false; }

    template <class Rep, class Period>
    static bool run_for(Counter&, std::chrono::duration<Rep, Period>)
    {
        return false;
    }
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::run Traits::MAX_LATENCY is adhered to")
{
    Counter counter{};
    const auto start = std::chrono::steady_clock::now();
    agrpc::run<MyWaitTraits>(grpc_context, counter,
                             [&, count = 0]() mutable
                             {
                                 ++count;
                                 return 6 == count;
                             });
    const auto end = std::chrono::steady_clock::now();
    CHECK_LE(std::chrono::seconds{1}, end - start);
}

TEST_CASE_FIXTURE(
    RunTest, "agrpc::run_completion_queue can process asio::post to io_context and ignores asio::post to grpc_context")
{
    const auto expected_thread = std::this_thread::get_id();
    bool invoked{false};
    bool has_posted{false};
    grpc::Alarm alarm;
    asio::post(io_context,
               [&]
               {
                   CHECK_EQ(std::this_thread::get_id(), expected_thread);
                   test::post(grpc_context,
                              [&]
                              {
                                  has_posted = true;
                              });
                   wait(alarm, test::ten_milliseconds_from_now(),
                        [&](bool)
                        {
                            CHECK_EQ(std::this_thread::get_id(), expected_thread);
                            invoked = true;
                            grpc_context.stop();
                        });
               });
    agrpc::run_completion_queue(grpc_context, io_context);
    CHECK(invoked);
    CHECK_FALSE(has_posted);
}