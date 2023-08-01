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

#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/use_awaitable.hpp>
#include <agrpc/wait.hpp>

#include <cstddef>
#include <thread>
#include <vector>

#ifdef AGRPC_TEST_ASIO_HAS_CONCEPTS
TEST_CASE("GrpcExecutor fulfills Executor TS concepts")
{
    CHECK(asio::execution::executor<agrpc::GrpcExecutor>);
    CHECK(asio::execution::executor_of<agrpc::GrpcExecutor, test::InvocableArchetype>);
}

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
TEST_CASE("GrpcSender and ScheduleSender fulfill std::execution concepts")
{
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcExecutor>()));
    using GrpcSender =
        decltype(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                             std::declval<UseSender>()));
    CHECK(asio::execution::sender<GrpcSender>);
    CHECK(asio::execution::typed_sender<GrpcSender>);
    CHECK(asio::execution::sender_to<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    using OperationState = asio::execution::connect_result_t<GrpcSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<OperationState>);

    using ScheduleSender = decltype(asio::execution::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(asio::execution::sender<ScheduleSender>);
    CHECK(asio::execution::typed_sender<ScheduleSender>);
    CHECK(asio::execution::sender_to<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    using ScheduleSenderOperationState = asio::execution::connect_result_t<ScheduleSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<ScheduleSenderOperationState>);
}
#endif
#endif

#ifndef AGRPC_USE_RECYCLING_ALLOCATOR
TEST_CASE_FIXTURE(
    test::GrpcContextTest,
    "asio BasicGrpcExecutor<PmrAllocator> can be constructed using allocator_traits<polymorphic_allocator>::construct")
{
    using Executor = agrpc::pmr::GrpcExecutor;
    std::vector<Executor, agrpc::detail::pmr::polymorphic_allocator<Executor>> vector;
    vector.emplace_back(grpc_context, agrpc::detail::pmr::new_delete_resource());
    CHECK_EQ(agrpc::detail::pmr::new_delete_resource(), vector.front().get_allocator().resource());
}
#endif

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "co_spawn two Alarms and await their ok")
{
    bool ok1{false};
    bool ok2{false};
    test::co_spawn_and_run(
        grpc_context,
        [&]() -> agrpc::GrpcAwaitable<void>
        {
            grpc::Alarm alarm;
            ok1 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
        },
        [&]() -> agrpc::GrpcAwaitable<void>
        {
            grpc::Alarm alarm;
            ok2 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
        });
    CHECK(ok1);
    CHECK(ok2);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "stop GrpcContext within awaitable while waiting for an Alarm")
{
    bool ok{true};
    grpc::Alarm alarm;
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    test::co_spawn_and_run(grpc_context,
                           [&]() -> agrpc::GrpcAwaitable<void>
                           {
                               agrpc::wait(alarm, test::five_seconds_from_now(),
                                           asio::bind_executor(grpc_context,
                                                               [&](bool wait_ok)
                                                               {
                                                                   ok = wait_ok;
                                                               }));
                               grpc_context.stop();
                               co_return;
                           });
    alarm.Cancel();
    CHECK(ok);
    grpc_context.run();
    CHECK_FALSE(ok);
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
}

TEST_CASE("destruct GrpcContext while co_await'ing an alarm")
{
    bool invoked{false};
    grpc::Alarm alarm;
    {
        agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
        test::post(grpc_context,
                   [&]
                   {
                       grpc_context.stop();
                   });
        test::co_spawn(grpc_context,
                       [&]() -> agrpc::GrpcAwaitable<void>
                       {
                           co_await agrpc::wait(alarm, test::hundred_milliseconds_from_now(),
                                                agrpc::GRPC_USE_AWAITABLE);
                           invoked = true;
                       });
        grpc_context.run();
        CHECK_FALSE(invoked);
        grpc_context.reset();
    }
}

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "call agrpc::wait from destructor of an awaitable while GrpcContext is being destructed")
{
    bool invoked{false};
    post(
        [&]()
        {
            grpc_context.stop();
        });
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::GrpcAwaitable<void>
                   {
                       auto alarm = std::make_shared<grpc::Alarm>();
                       agrpc::detail::ScopeGuard guard{[&, alarm]
                                                       {
                                                           wait(*alarm, test::one_second_from_now(),
                                                                [&, alarm](bool)
                                                                {
                                                                    invoked = true;
                                                                });
                                                       }};
                       co_await agrpc::wait(*alarm, test::hundred_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK_FALSE(invoked);
    grpc_context.reset();
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "co_await Alarm with asio::awaitable<>")
{
    bool ok{false};
    test::co_spawn(get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), asio::use_awaitable);
                   });
    grpc_context.run();
    CHECK(ok);
}

#ifndef AGRPC_USE_RECYCLING_ALLOCATOR
TEST_CASE_FIXTURE(test::GrpcContextTest, "co_await Alarm with pmr::GRPC_USE_AWAITABLE")
{
    bool ok{false};
    test::co_spawn(asio::require(get_executor(),
                                 asio::execution::allocator(agrpc::detail::pmr::polymorphic_allocator<std::byte>())),
                   [&]() -> agrpc::pmr::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(),
                                                 agrpc::pmr::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(ok);
}
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with awaitable operators")
{
    std::size_t result_index{};
    grpc::Alarm alarm;
    asio::steady_timer timer{get_executor(), std::chrono::milliseconds(100)};
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       using namespace asio::experimental::awaitable_operators;
                       const auto variant = co_await (timer.async_wait(asio::use_awaitable) ||
                                                      agrpc::wait(alarm, test::five_seconds_from_now()));
                       result_index = variant.index();
                   });
    grpc_context.run();
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, result_index);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_executor can be used to await Alarm from an io_context")
{
    bool ok{false};
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
    std::optional guard{get_work_tracking_executor()};
    asio::io_context io_context;
    test::co_spawn(io_context,
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(),
                                                 asio::bind_executor(grpc_context, asio::use_awaitable));
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

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "bind_executor can be used to switch to io_context when awaiting asio::steady_timer from GrpcContext")
{
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
    asio::io_context io_context;
    std::optional guard{asio::require(io_context.get_executor(), asio::execution::outstanding_work_t::tracked)};
    asio::steady_timer timer{io_context};
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       timer.expires_after(std::chrono::milliseconds(10));
                       co_await timer.async_wait(asio::bind_executor(io_context, asio::use_awaitable));
                       actual_thread_id = std::this_thread::get_id();
                       guard.reset();
                   });
    std::thread io_context_thread{[&]
                                  {
                                      expected_thread_id = std::this_thread::get_id();
                                      io_context.run();
                                  }};
    grpc_context.run();
    io_context_thread.join();
    CHECK_EQ(expected_thread_id, actual_thread_id);
}

// Clang struggles to correctly optimize around thread switches on suspension points.
auto
#if defined(__clang__)
    __attribute__((optnone))
#endif
    get_thread_id()
{
    return std::this_thread::get_id();
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_executor can be used to switch to thread_pool and back to GrpcContext")
{
    std::thread::id actual_grpc_context_thread_id{};
    std::thread::id thread_pool_thread_id{};
    asio::thread_pool thread_pool{1};
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       co_await asio::post(asio::bind_executor(thread_pool, asio::use_awaitable));
                       thread_pool_thread_id = get_thread_id();
                       co_await asio::post(asio::use_awaitable);
                       actual_grpc_context_thread_id = get_thread_id();
                   });
    const auto expected_grpc_context_thread_id = std::this_thread::get_id();
    grpc_context.run();
    thread_pool.join();
    CHECK_NE(expected_grpc_context_thread_id, thread_pool_thread_id);
    CHECK_EQ(expected_grpc_context_thread_id, actual_grpc_context_thread_id);
}
#endif
#endif