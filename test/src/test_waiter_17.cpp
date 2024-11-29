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

#include "utils/doctest.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/io_context_test.hpp"
#include "utils/time.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/waiter.hpp>

template <class Singature>
inline constexpr auto IMMEDIATE = 0;

template <class... Args>
inline constexpr auto IMMEDIATE<void(Args...)> = [](auto&&, auto&& ch)
{
    std::move(ch)(Args{}...);
};

TEST_CASE_TEMPLATE("Waiter: wait before initiate", T, void(bool), void(test::ErrorCode, bool))
{
    agrpc::GrpcContext grpc_context;
    bool ok{};
    agrpc::Waiter<T, asio::any_io_executor> waiter;
    waiter.wait(
        [&](test::ErrorCode ec, auto&&...)
        {
            ok = !ec;
        });
    waiter.initiate(IMMEDIATE<T>, grpc_context);
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_TEMPLATE("Waiter: wait for already completed operation", T, void(bool), void(test::ErrorCode, bool))
{
    agrpc::GrpcContext grpc_context;
    bool ok{};
    agrpc::Waiter<T, asio::any_io_executor> waiter;
    waiter.initiate(IMMEDIATE<T>, grpc_context.get_executor());
    waiter.wait(
        [&](test::ErrorCode ec, auto&&...)
        {
            CHECK(grpc_context.get_executor().running_in_this_thread());
            ok = !ec;
        });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::IoContextTest, "Waiter: can handle move-only completion arguments")
{
    agrpc::Waiter<void(std::unique_ptr<int>), asio::io_context::executor_type> waiter;
    waiter.initiate(
        [&](auto&&, auto&& ch)
        {
            asio::post(io_context,
                       [ch = std::move(ch)]() mutable
                       {
                           std::move(ch)(std::make_unique<int>(42));
                       });
        },
        io_context);
    waiter.wait(
        [&](test::ErrorCode ec, std::unique_ptr<int>&& actual)
        {
            CHECK_FALSE(ec);
            CHECK_EQ(42, *actual);
        });
    io_context.run();
}

inline constexpr auto alarm_wait = [](agrpc::Alarm& alarm, auto deadline, auto token)
{
    return alarm.wait(deadline, token);
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "Waiter: initiate alarm -> cancel alarm -> wait returns false")
{
    agrpc::Waiter<void(bool)> waiter;
    agrpc::Alarm alarm{grpc_context};
    waiter.initiate(alarm_wait, alarm, test::five_seconds_from_now());
    CHECK_FALSE(waiter.is_ready());
    alarm.cancel();
    waiter.wait(
        [&](auto&& ec, bool ok)
        {
            CHECK_FALSE(ec);
            CHECK_FALSE(ok);
            CHECK(waiter.is_ready());
        });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "Waiter: can change default completion token")
{
    agrpc::UseSender::as_default_on_t<agrpc::Waiter<void(bool)>> waiter;
    agrpc::Alarm alarm{grpc_context};
    waiter.initiate(alarm_wait, alarm, test::ten_milliseconds_from_now());
    auto sender = waiter.wait();
    CHECK(agrpc::detail::exec::is_sender_v<decltype(sender)>);
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::IoContextTest, "Waiter: can handle lots of completion arguments")
{
    using Signature = void(int, int, bool, double, float, char);
    agrpc::Waiter<Signature, asio::any_io_executor> waiter;
    waiter.initiate(
        [&](auto&&, auto&& ch)
        {
            asio::post(io_context,
                       [ch = std::move(ch)]() mutable
                       {
                           std::move(ch)(42, 1, true, 0.5, 0.25f, 'a');
                       });
        },
        io_context);
    waiter.wait(
        [&](test::ErrorCode ec, int a, int, bool, double, float, char c)
        {
            CHECK_FALSE(ec);
            CHECK_EQ(42, a);
            CHECK_EQ('a', c);
        });
    io_context.run();
}

#ifdef AGRPC_TEST_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "Waiter: cancel wait for alarm and wait again")
{
    bool done{};
    agrpc::Waiter<void(bool)> waiter;
    agrpc::Alarm alarm{grpc_context};
    waiter.initiate(alarm_wait, alarm, test::five_hundred_milliseconds_from_now());
    asio::cancellation_signal signal;
    waiter.wait(agrpc::detail::AllocatorBinder(get_allocator(), asio::bind_cancellation_slot(signal.slot(),
                                                                                             [&](auto&& ec, bool)
                                                                                             {
                                                                                                 done = !ec;
                                                                                             })));
    signal.emit(asio::cancellation_type::terminal);
    waiter.wait(
        [&](auto&&, bool)
        {
            CHECK_FALSE(done);
            done = true;
        });
    grpc_context.run();
    CHECK(done);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::IoContextTest, "Waiter: wait for asio::steady_timer")
{
    agrpc::Waiter<void(test::ErrorCode), asio::any_io_executor> waiter;
    asio::steady_timer timer{io_context, std::chrono::minutes(5)};
    waiter.initiate(
        [&](asio::steady_timer& timer, auto&& ch)
        {
            timer.async_wait(std::move(ch));
        },
        timer);
    asio::cancellation_signal signal;
    waiter.wait(asio::bind_cancellation_slot(signal.slot(),
                                             [&](test::ErrorCode ec)
                                             {
                                                 CHECK_EQ(asio::error::operation_aborted, ec);
                                                 CHECK_EQ(1, timer.cancel());
                                             }));
    signal.emit(asio::cancellation_type::all);
    io_context.run();
}
#endif