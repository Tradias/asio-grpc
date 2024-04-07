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

#include <agrpc/alarm.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

#include <cstddef>
#include <thread>
#include <vector>

#ifdef AGRPC_TEST_HAS_STD_PMR
#include <memory_resource>
#endif

#ifdef AGRPC_TEST_ASIO_HAS_CONCEPTS
TEST_CASE("GrpcExecutor fulfills Executor TS concepts") { CHECK(asio::execution::executor<agrpc::GrpcExecutor>); }
#endif

#ifdef AGRPC_TEST_HAS_STD_PMR
TEST_CASE_FIXTURE(
    test::GrpcContextTest,
    "asio BasicGrpcExecutor<PmrAllocator> can be constructed using allocator_traits<polymorphic_allocator>::construct")
{
    using Executor = decltype(asio::require(std::declval<agrpc::GrpcExecutor>(),
                                            asio::execution::allocator(std::pmr::polymorphic_allocator<std::byte>{})));
    std::vector<Executor, std::pmr::polymorphic_allocator<Executor>> vector;
    vector.emplace_back(grpc_context, std::pmr::new_delete_resource());
    CHECK_EQ(std::pmr::new_delete_resource(), vector.front().get_allocator().resource());
}
#endif

#ifdef AGRPC_TEST_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "stop GrpcContext from awaitable while waiting for an Alarm")
{
    bool ok{true};
    agrpc::Alarm alarm{grpc_context};
    const auto not_to_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    test::co_spawn_and_run(grpc_context,
                           [&]() -> asio::awaitable<void>
                           {
                               test::wait(alarm, test::five_seconds_from_now(),
                                          [&](bool wait_ok)
                                          {
                                              ok = wait_ok;
                                          });
                               grpc_context.stop();
                               co_return;
                           });
    alarm.cancel();
    CHECK(ok);
    grpc_context.run();
    CHECK_FALSE(ok);
    CHECK_GT(not_to_exceed, std::chrono::steady_clock::now());
}

TEST_CASE("destruct GrpcContext while co_await'ing an alarm")
{
    bool invoked{false};
    std::optional<agrpc::Alarm> alarm;
    {
        agrpc::GrpcContext grpc_context;
        alarm.emplace(grpc_context);
        test::post(grpc_context,
                   [&]
                   {
                       grpc_context.stop();
                   });
        test::co_spawn(grpc_context,
                       [&]() -> asio::awaitable<void>
                       {
                           co_await alarm->wait(test::hundred_milliseconds_from_now(), asio::use_awaitable);
                           invoked = true;
                       });
        grpc_context.run();
        CHECK_FALSE(invoked);
        grpc_context.reset();
    }
}

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "wait for Alarm from destructor of an awaitable while GrpcContext is being destructed")
{
    bool invoked{false};
    post(
        [&]()
        {
            grpc_context.stop();
        });
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       auto alarm = std::make_shared<agrpc::Alarm>(grpc_context);
                       agrpc::detail::ScopeGuard guard{[&, alarm]
                                                       {
                                                           alarm->wait(test::one_second_from_now(),
                                                                       [&, alarm](bool)
                                                                       {
                                                                           invoked = true;
                                                                       });
                                                       }};
                       co_await alarm->wait(test::hundred_milliseconds_from_now(), asio::use_awaitable);
                   });
    grpc_context.run();
    CHECK_FALSE(invoked);
    grpc_context.reset();
}

#ifdef AGRPC_TEST_ASIO_HAS_CANCELLATION_SLOT
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