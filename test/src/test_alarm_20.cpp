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

#include <agrpc/alarm.hpp>

#ifdef AGRPC_TEST_HAS_STD_PMR
#include <memory_resource>
#endif

#ifdef AGRPC_TEST_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "co_spawn two Alarms and await their ok using GrpcExecutor")
{
    using GrpcAwaitable = asio::awaitable<void, agrpc::GrpcExecutor>;
    constexpr asio::use_awaitable_t<agrpc::GrpcExecutor> GRPC_USE_AWAITABLE{};
    bool ok1{false};
    bool ok2{false};
    test::co_spawn_and_run(
        grpc_context,
        [&]() -> GrpcAwaitable
        {
            agrpc::Alarm alarm{grpc_context};
            ok1 = co_await alarm.wait(test::ten_milliseconds_from_now(), GRPC_USE_AWAITABLE);
        },
        [&]() -> GrpcAwaitable
        {
            agrpc::Alarm alarm{grpc_context};
            ok2 = co_await alarm.wait(test::ten_milliseconds_from_now(), GRPC_USE_AWAITABLE);
        });
    CHECK(ok1);
    CHECK(ok2);
}

#ifdef AGRPC_TEST_HAS_STD_PMR
TEST_CASE_FIXTURE(test::GrpcContextTest, "co_await Alarm with GrpcExecutor<std::pmr::polymorphic_allocator>")
{
    bool ok{false};
    auto executor =
        asio::require(get_executor(), asio::execution::allocator(std::pmr::polymorphic_allocator<std::byte>()));
    test::co_spawn(executor,
                   [&]() -> asio::awaitable<void, decltype(executor)>
                   {
                       agrpc::Alarm alarm{grpc_context};
                       ok = co_await alarm.wait(test::ten_milliseconds_from_now(),
                                                asio::use_awaitable_t<decltype(executor)>{});
                   });
    grpc_context.run();
    CHECK(ok);
}
#endif

#ifdef AGRPC_TEST_ASIO_HAS_CANCELLATION_SLOT
// In older versions of asio `asio::bind_executor` cannot handle use_awaitable
TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "bind_executor can be used to control context switches while waiting for an Alarm from an io_context")
{
    bool ok{false};
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
    std::optional guard{get_work_tracking_executor()};
    asio::io_context io_context;
    test::co_spawn(io_context,
                   [&]() -> asio::awaitable<void>
                   {
                       agrpc::Alarm alarm{grpc_context};
                       ok = co_await alarm.wait(test::ten_milliseconds_from_now(),
                                                asio::bind_executor(asio::system_executor{}, asio::use_awaitable));
                       actual_thread_id = std::this_thread::get_id();
                       guard.reset();
                   });
    std::thread grpc_context_thread{[&]
                                    {
                                        expected_thread_id = std::this_thread::get_id();
                                        grpc_context.run();
                                    }};
    io_context.run();
    grpc_context_thread.join();
    CHECK(ok);
    CHECK_EQ(expected_thread_id, actual_thread_id);
}
#endif

#ifdef AGRPC_TEST_ASIO_PARALLEL_GROUP
TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with awaitable operators")
{
    std::size_t result_index{};
    agrpc::Alarm alarm{grpc_context};
    asio::steady_timer timer{get_executor(), std::chrono::milliseconds(100)};
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       using namespace asio::experimental::awaitable_operators;
                       const auto variant = co_await (timer.async_wait(asio::use_awaitable) ||
                                                      alarm.wait(test::five_seconds_from_now(), asio::use_awaitable));
                       result_index = variant.index();
                   });
    grpc_context.run();
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, result_index);
}
#endif
#endif
