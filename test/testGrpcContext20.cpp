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
#include "utils/time.hpp"

#include <agrpc/getCompletionQueue.hpp>
#include <agrpc/useAwaitable.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
#ifdef AGRPC_ASIO_HAS_CONCEPTS
TEST_CASE("GrpcExecutor fulfills Executor TS concepts")
{
    CHECK(asio::execution::executor<agrpc::GrpcExecutor>);
    CHECK(asio::execution::executor_of<agrpc::GrpcExecutor, test::InvocableArchetype>);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE("asio-grpc fulfills unified executor concepts")
{
    CHECK(asio::execution::is_scheduler_v<agrpc::GrpcExecutor>);
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcExecutor>()));
    using UseSenderFromGrpcContext = decltype(agrpc::use_sender(std::declval<agrpc::GrpcContext&>()));
    CHECK(std::is_same_v<UseSender, UseSenderFromGrpcContext>);
    using GrpcSender =
        decltype(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                             std::declval<UseSender>()));
    CHECK(asio::execution::sender<GrpcSender>);
    CHECK(asio::execution::is_sender_v<GrpcSender>);
    CHECK(asio::execution::typed_sender<GrpcSender>);
    CHECK(asio::execution::is_typed_sender_v<GrpcSender>);
    CHECK(asio::execution::sender_to<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_sender_to_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_nothrow_connect_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    using OperationState = asio::execution::connect_result_t<GrpcSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<OperationState>);
    CHECK(asio::execution::is_operation_state_v<OperationState>);

    using ScheduleSender = decltype(asio::execution::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(asio::execution::sender<ScheduleSender>);
    CHECK(asio::execution::is_sender_v<ScheduleSender>);
    CHECK(asio::execution::typed_sender<ScheduleSender>);
    CHECK(asio::execution::is_typed_sender_v<ScheduleSender>);
    CHECK(asio::execution::sender_to<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_sender_to_v<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_nothrow_connect_v<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    using ScheduleSenderOperationState = asio::execution::connect_result_t<ScheduleSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<ScheduleSenderOperationState>);
    CHECK(asio::execution::is_operation_state_v<ScheduleSenderOperationState>);
}
#endif
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "asio GrpcExecutor::schedule")
{
    bool is_invoked{false};
    auto sender = asio::execution::schedule(get_executor());
    test::FunctionAsReceiver receiver{[&]
                                      {
                                          is_invoked = true;
                                      }};
    auto operation_state = asio::execution::connect(std::move(sender), receiver);
    asio::execution::start(operation_state);
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
    CHECK_FALSE(receiver.was_done);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio GrpcExecutor::submit with allocator")
{
    asio::execution::submit(asio::execution::schedule(get_executor()),
                            test::FunctionAsReceiver{[] {}, get_allocator()});
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::execution connect and start Alarm")
{
    bool ok{false};
    grpc::Alarm alarm;
    auto wait_sender = agrpc::wait(alarm, test::ten_milliseconds_from_now(), use_sender());
    test::FunctionAsReceiver receiver{[&](bool wait_ok)
                                      {
                                          ok = wait_ok;
                                      }};
    auto operation_state = asio::execution::connect(std::move(wait_sender), std::move(receiver));
    asio::execution::start(operation_state);
    grpc_context.run();
    CHECK(ok);
}
#endif

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "get_completion_queue")
{
    grpc::CompletionQueue* queue{};
    SUBCASE("GrpcAwaitable")
    {
        test::co_spawn(grpc_context,
                       [&]() -> agrpc::GrpcAwaitable<void>
                       {
                           queue = co_await agrpc::get_completion_queue(agrpc::GRPC_USE_AWAITABLE);
                       });
    }
    SUBCASE("asio::awaitable")
    {
        test::co_spawn(grpc_context,
                       [&]() -> asio::awaitable<void>
                       {
                           queue = co_await agrpc::get_completion_queue();
                       });
    }
    grpc_context.run();
    CHECK_EQ(grpc_context.get_completion_queue(), queue);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "co_spawn two Alarms and await their ok")
{
    bool ok1{false};
    bool ok2{false};
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok1 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       grpc_context.stop();
                   });
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok2 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(ok1);
    CHECK(ok2);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "destruct GrpcContext while co_await'ing an alarm")
{
    bool invoked{false};
    asio::post(grpc_context,
               [&]()
               {
                   grpc_context.stop();
               });
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       co_await agrpc::wait(alarm, test::hundred_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       invoked = true;
                   });
    grpc_context.run();
    CHECK_FALSE(invoked);
    grpc_context.reset();
}

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "call agrpc::wait from destructor of an awaitable while GrpcContext is being destructed")
{
    bool invoked{false};
    asio::post(grpc_context,
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
                                                           agrpc::wait(*alarm, test::one_seconds_from_now(),
                                                                       asio::bind_executor(grpc_context,
                                                                                           [&, alarm](bool)
                                                                                           {
                                                                                               invoked = true;
                                                                                           }));
                                                       }};
                       co_await agrpc::wait(*alarm, test::hundred_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK_FALSE(invoked);
    grpc_context.reset();
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with allocator")
{
    test::co_spawn(get_pmr_executor(),
                   [&]() -> agrpc::pmr::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::pmr::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with asio::awaitable<>")
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

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with awaitable operators")
{
    std::size_t result_index{};
    grpc::Alarm alarm;
    asio::steady_timer timer{get_executor(), std::chrono::milliseconds(100)};
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       using namespace asio::experimental::awaitable_operators;
                       const auto variant = co_await(timer.async_wait(asio::use_awaitable) ||
                                                     agrpc::wait(alarm, test::five_seconds_from_now()));
                       result_index = variant.index();
                   });
    grpc_context.run();
    CHECK_GT(not_too_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, result_index);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_executor can be used to await Alarm from an io_context")
{
    bool ok{false};
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
    auto guard = asio::make_work_guard(grpc_context);
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
                  "bind_executor can be used to switch to io_context when awaiting asio::steady_timer")
{
    std::thread::id expected_thread_id{};
    std::thread::id actual_thread_id{};
    asio::io_context io_context;
    auto guard = asio::make_work_guard(io_context);
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

#ifndef __clang__
TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_executor can be used to switch to thread_pool and back to GrpcContext")
{
    std::thread::id actual_grpc_context_thread_id{};
    std::thread::id thread_pool_thread_id{};
    asio::thread_pool thread_pool{1};
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       co_await asio::post(asio::bind_executor(thread_pool, asio::use_awaitable));
                       thread_pool_thread_id = std::this_thread::get_id();
                       co_await asio::post(asio::use_awaitable);
                       actual_grpc_context_thread_id = std::this_thread::get_id();
                   });
    const auto grpc_context_thread_id = std::this_thread::get_id();
    grpc_context.run();
    thread_pool.join();
    CHECK_NE(grpc_context_thread_id, thread_pool_thread_id);
    CHECK_EQ(grpc_context_thread_id, actual_grpc_context_thread_id);
}
#endif
#endif
#endif
}