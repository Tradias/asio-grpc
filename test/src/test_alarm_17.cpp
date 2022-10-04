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
#include "utils/exception.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/time.hpp"

#include <agrpc/alarm.hpp>

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post a apgrc::Alarm and use variadic-arg callback for its wait")
{
    bool ok{false};
    agrpc::Alarm alarm{grpc_context};
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

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::Alarm with const-ref callback")
{
    agrpc::Alarm alarm{grpc_context};
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

    void operator()(agrpc::Alarm&&, bool wait_ok) const noexcept { ok = wait_ok; }

    bool& ok;
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::Alarm move-overload with callback")
{
    bool ok{false};
    agrpc::Alarm alarm{grpc_context};
    std::move(alarm).wait(test::ten_milliseconds_from_now(),
                          [&](agrpc::Alarm&& alarm, bool wait_ok)
                          {
                              CHECK(wait_ok);
                              std::move(alarm).wait(test::ten_milliseconds_from_now(), WaitOkAssigner{ok});
                          });
    grpc_context.run();
    CHECK(ok);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
template <class UseMove, class T>
auto&& move_if(T&& t)
{
    if constexpr (UseMove::value)
    {
        return std::move(t);
    }
    else
    {
        return t;
    }
}

TEST_CASE_TEMPLATE("cancel agrpc::Alarm with cancellation_type::total", T, std::true_type, std::false_type)
{
    test::GrpcContextTest test;
    bool ok{true};
    asio::cancellation_signal signal{};
    agrpc::Alarm alarm{test.grpc_context};
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    move_if<T>(alarm).wait(test::five_seconds_from_now(),
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
    CHECK_GT(not_too_exceed, std::chrono::steady_clock::now());
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
            move_if<T>(alarm).wait(test::hundred_milliseconds_from_now(),
                                   asio::bind_cancellation_slot(
                                       signal.slot(), asio::bind_executor(test.get_executor(), WaitOkAssigner{ok})));
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
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    test::parallel_group_bind_executor(
        get_executor(), asio::experimental::wait_for_one(),
        [&](std::array<std::size_t, 2> actual_completion_order, test::ErrorCode timer_ec, bool wait_ok)
        {
            completion_order = actual_completion_order;
            error_code.emplace(timer_ec);
            ok = wait_ok;
        },
        [&](auto&& t)
        {
            return timer.async_wait(std::move(t));
        },
        [&](auto&& t)
        {
            return alarm.wait(test::five_seconds_from_now(), std::move(t));
        });
    grpc_context.run();
    CHECK_GT(not_too_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, completion_order[0]);
    CHECK_EQ(1, completion_order[1]);
    CHECK_EQ(test::ErrorCode{}, error_code);
    CHECK_FALSE(ok);
}
#endif