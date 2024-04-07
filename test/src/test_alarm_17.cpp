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

#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/time.hpp"
#include "utils/utility.hpp"

#include <agrpc/alarm.hpp>

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post a apgrc::Alarm and use variadic-arg callback for its wait")
{
    bool ok{false};
    agrpc::BasicAlarm alarm{grpc_context};
    post(
        [&]
        {
            alarm.wait(test::ten_milliseconds_from_now(),
                       [&](auto&&... args)
                       {
                           ok = bool{args...};
                       });
        });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::spawn an Alarm")
{
    bool ok{false};
    bool use_yield_context = false;
    SUBCASE("asio::yield_context") { use_yield_context = true; }
    SUBCASE("asio::basic_yield_context") {}
    test::typed_spawn(get_executor(),
                      [&](auto&& yield)
                      {
                          agrpc::Alarm alarm{grpc_context};
                          if (use_yield_context)
                          {
                              ok = alarm.wait(test::hundred_milliseconds_from_now(), asio::yield_context(yield));
                          }
                          else
                          {
                              ok = alarm.wait(test::hundred_milliseconds_from_now(), yield);
                          }
                      });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::Alarm with const-ref callback")
{
    agrpc::BasicAlarm alarm{grpc_context.get_executor()};
    bool ok{false};
    const auto cb = [&](bool wait_ok)
    {
        ok = wait_ok;
    };
    alarm.wait(test::ten_milliseconds_from_now(), cb);
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::Alarm with move-only callback")
{
    struct Cb
    {
        std::unique_ptr<int>& target;
        std::unique_ptr<int> ptr{std::make_unique<int>(42)};

        void operator()(bool) { target = std::move(ptr); }
    };
    agrpc::Alarm alarm{grpc_context};
    std::unique_ptr<int> ptr;
    alarm.wait(test::ten_milliseconds_from_now(), Cb{ptr});
    grpc_context.run();
    REQUIRE(ptr);
    CHECK_EQ(42, *ptr);
}

struct WaitOkAssigner
{
    void operator()(bool wait_ok) const noexcept { ok = wait_ok; }

    void operator()(bool wait_ok, agrpc::Alarm&&) const noexcept { ok = wait_ok; }

    bool& ok;
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::Alarm move-overload with callback")
{
    bool ok{false};
    agrpc::Alarm(grpc_context)
        .wait(test::ten_milliseconds_from_now(),
              [&](bool wait_ok, agrpc::Alarm&& alarm)
              {
                  CHECK(wait_ok);
                  std::move(alarm).wait(test::ten_milliseconds_from_now(), WaitOkAssigner{ok});
              });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::Alarm::cancel")
{
    bool ok{true};
    agrpc::Alarm alarm{grpc_context};
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    alarm.wait(test::five_seconds_from_now(), WaitOkAssigner{ok});
    post(
        [&]
        {
            alarm.cancel();
            alarm.cancel();
        });
    grpc_context.run();
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
    CHECK_FALSE(ok);
}

#ifdef AGRPC_TEST_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::deferred with Alarm")
{
    bool ok1{false};
    bool ok2{false};
    agrpc::Alarm alarm{grpc_context};
    auto deferred_op = alarm.wait(test::ten_milliseconds_from_now(),
                                  test::ASIO_DEFERRED(
                                      [&](bool wait_ok)
                                      {
                                          ok1 = wait_ok;
                                          return alarm.wait(test::ten_milliseconds_from_now(), test::ASIO_DEFERRED);
                                      }));
    std::move(deferred_op)(
        [&](bool wait_ok)
        {
            ok2 = wait_ok;
        });
    grpc_context.run();
    CHECK(ok1);
    CHECK(ok2);
}

TEST_CASE_TEMPLATE("cancel agrpc::Alarm with cancellation_type::total", T, std::true_type, std::false_type)
{
    test::GrpcContextTest test;
    bool ok{true};
    asio::cancellation_signal signal{};
    agrpc::Alarm alarm{test.grpc_context};
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    test::move_if<T>(alarm).wait(test::five_seconds_from_now(),
                                 asio::bind_cancellation_slot(signal.slot(), WaitOkAssigner{ok}));
    test.post(
        [&]
        {
            signal.emit(asio::cancellation_type::total);
        });
    SUBCASE("cancel once") {}
    SUBCASE("cancel twice has no effect")
    {
        test.post(
            [&]
            {
                signal.emit(asio::cancellation_type::total);
            });
    }
    test.grpc_context.run();
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
    CHECK_FALSE(ok);
}

TEST_CASE_TEMPLATE("cancel agrpc::Alarm with cancellation_type::none", T, std::true_type, std::false_type)
{
    test::GrpcContextTest test;
    bool ok{false};
    asio::cancellation_signal signal{};
    agrpc::Alarm alarm{test.grpc_context};
    test.post(
        [&]
        {
            test::move_if<T>(alarm).wait(test::hundred_milliseconds_from_now(),
                                         asio::bind_cancellation_slot(signal.slot(), WaitOkAssigner{ok}));
            test.post(
                [&]
                {
                    signal.emit(asio::cancellation_type::none);
                });
        });
    test.grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel agrpc::Alarm with parallel_group")
{
    std::array<std::size_t, 2> completion_order;
    std::optional<test::ErrorCode> error_code;
    bool ok{true};
    agrpc::Alarm alarm{grpc_context};
    asio::steady_timer timer{get_executor(), std::chrono::milliseconds(100)};
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    asio::experimental::make_parallel_group(
        [&](auto&& t)
        {
            return timer.async_wait(std::move(t));
        },
        [&](auto&& t)
        {
            return alarm.wait(test::five_seconds_from_now(), std::move(t));
        })
        .async_wait(asio::experimental::wait_for_one(),
                    [&](std::array<std::size_t, 2> actual_completion_order, test::ErrorCode timer_ec, bool wait_ok)
                    {
                        completion_order = actual_completion_order;
                        error_code.emplace(timer_ec);
                        ok = wait_ok;
                    });
    grpc_context.run();
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, completion_order[0]);
    CHECK_EQ(1, completion_order[1]);
    CHECK_EQ(test::ErrorCode{}, error_code);
    CHECK_FALSE(ok);
}
#endif

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::execution connect and start Alarm")
{
    bool ok{false};
    agrpc::Alarm alarm{grpc_context};
    auto wait_sender = alarm.wait(test::ten_milliseconds_from_now(), agrpc::use_sender);
    test::FunctionAsReceiver receiver{[&]()
                                      {
                                          ok = true;
                                      }};
    auto operation_state = std::move(wait_sender).connect(std::move(receiver));
    operation_state.start();
    grpc_context.run();
    CHECK(ok);
}