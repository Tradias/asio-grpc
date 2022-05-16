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
#include "utils/time.hpp"

#include <agrpc/grpcContext.hpp>
#include <agrpc/wait.hpp>

#include <thread>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE("GrpcExecutor fulfills Executor TS traits")
{
    using Exec = agrpc::GrpcContext::executor_type;
    CHECK(asio::execution::can_execute_v<std::add_const_t<Exec>, asio::execution::invocable_archetype>);
    CHECK(asio::execution::is_executor_v<Exec>);
    CHECK(asio::can_require_v<Exec, asio::execution::blocking_t::never_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::blocking_t::possibly_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::relationship_t::fork_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::relationship_t::continuation_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::outstanding_work_t::tracked_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::outstanding_work_t::untracked_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::allocator_t<agrpc::detail::pmr::polymorphic_allocator<std::byte>>>);
    CHECK(asio::can_query_v<Exec, asio::execution::blocking_t>);
    CHECK(asio::can_query_v<Exec, asio::execution::relationship_t>);
    CHECK(asio::can_query_v<Exec, asio::execution::outstanding_work_t>);
    CHECK(asio::can_query_v<Exec, asio::execution::mapping_t>);
    CHECK(asio::can_query_v<Exec, asio::execution::allocator_t<void>>);
    CHECK(asio::can_query_v<Exec, asio::execution::context_t>);
    CHECK(std::is_constructible_v<asio::any_io_executor, Exec>);
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    auto executor = grpc_context.get_executor();
    auto possibly_blocking_executor = asio::require(executor, asio::execution::blocking_t::possibly);
    CHECK_EQ(asio::execution::blocking_t::possibly, asio::query(possibly_blocking_executor, asio::execution::blocking));
    CHECK_EQ(asio::execution::blocking_t::never,
             asio::query(asio::require(possibly_blocking_executor, asio::execution::blocking_t::never),
                         asio::execution::blocking));
    auto continuation_executor = asio::prefer(executor, asio::execution::relationship_t::continuation);
    CHECK_EQ(asio::execution::relationship_t::fork,
             asio::query(asio::prefer(continuation_executor, asio::execution::relationship_t::fork),
                         asio::execution::relationship));
    auto tracked_executor = asio::prefer(executor, asio::execution::outstanding_work_t::tracked);
    CHECK_EQ(asio::execution::outstanding_work_t::tracked,
             asio::query(tracked_executor, asio::execution::outstanding_work));
    CHECK_EQ(asio::execution::outstanding_work_t::untracked,
             asio::query(asio::prefer(tracked_executor, asio::execution::outstanding_work_t::untracked),
                         asio::execution::outstanding_work));
}

TEST_CASE("GrpcExecutor is mostly trivial")
{
    CHECK(std::is_trivially_copy_constructible_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_move_constructible_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_destructible_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_copy_assignable_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_move_assignable_v<agrpc::GrpcExecutor>);
    CHECK_EQ(sizeof(void*), sizeof(agrpc::GrpcExecutor));
}

TEST_CASE("GrpcExecutorOptions")
{
    CHECK(agrpc::detail::is_blocking_never(agrpc::detail::GrpcExecutorOptions::BLOCKING_NEVER));
    CHECK_FALSE(agrpc::detail::is_blocking_never(agrpc::detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED));
    CHECK(agrpc::detail::is_blocking_never(
        agrpc::detail::set_blocking_never(agrpc::detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED, true)));
    CHECK_FALSE(agrpc::detail::is_blocking_never(
        agrpc::detail::set_blocking_never(agrpc::detail::GrpcExecutorOptions::BLOCKING_NEVER, false)));

    CHECK(agrpc::detail::is_outstanding_work_tracked(agrpc::detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED));
    CHECK_FALSE(agrpc::detail::is_outstanding_work_tracked(agrpc::detail::GrpcExecutorOptions::BLOCKING_NEVER));
    CHECK(agrpc::detail::is_outstanding_work_tracked(
        agrpc::detail::set_outstanding_work_tracked(agrpc::detail::GrpcExecutorOptions::BLOCKING_NEVER, true)));
    CHECK_FALSE(agrpc::detail::is_outstanding_work_tracked(agrpc::detail::set_outstanding_work_tracked(
        agrpc::detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED, false)));
}

TEST_CASE("Work tracking GrpcExecutor constructor and assignment")
{
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    agrpc::GrpcContext other_context{std::make_unique<grpc::CompletionQueue>()};

    auto other_ex = asio::prefer(grpc_context.get_executor(), asio::execution::blocking_t::possibly);
    auto ex = asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked,
                            asio::execution::allocator(agrpc::detail::pmr::polymorphic_allocator<std::byte>()));
    auto ex2a = asio::require(ex, asio::execution::allocator);
    CHECK_EQ(std::allocator<void>{}, asio::query(ex2a, asio::execution::allocator));
    CHECK_NE(other_ex, ex2a);
    CHECK_NE(grpc_context.get_executor(), ex2a);
    CHECK_NE(other_context.get_executor(), ex2a);

    const auto ex1{ex};
    auto ex2{ex};
    auto ex3{std::move(ex)};
    CHECK_EQ(ex3, ex2);
    ex2 = ex1;
    CHECK_EQ(ex2, ex1);
    ex2 = std::move(ex3);
    ex2 = std::move(ex2);
    CHECK_EQ(ex2, ex1);
    ex3 = std::move(ex2);
    CHECK_EQ(ex3, ex1);
    ex2 = std::as_const(ex3);
    ex2 = std::as_const(ex2);
    CHECK_EQ(ex2, ex1);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext::reset")
{
    bool ok{false};
    CHECK_FALSE(grpc_context.is_stopped());
    asio::post(grpc_context,
               [&]
               {
                   ok = true;
                   CHECK_FALSE(grpc_context.is_stopped());
               });
    grpc_context.run();
    CHECK(grpc_context.is_stopped());
    CHECK(ok);
    grpc_context.reset();
    CHECK_FALSE(grpc_context.is_stopped());
    grpc_context.stop();
    asio::post(grpc_context,
               [&]
               {
                   ok = false;
               });
    grpc_context.run();
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext::stop does not complete pending operations")
{
    bool ok{false};
    asio::post(grpc_context,
               [&]
               {
                   grpc_context.stop();
                   asio::post(grpc_context,
                              [&]
                              {
                                  ok = true;
                              });
               });
    CHECK(grpc_context.run());
    CHECK_FALSE(ok);
}

TEST_CASE("GrpcContext::stop while waiting for Alarm will not invoke the Alarm's completion handler")
{
    bool is_stop_from_same_thread{true};
    SUBCASE("stop from same thread") {}
    SUBCASE("stop from other thread") { is_stop_from_same_thread = false; }
    bool ok{false};
    {
        std::thread thread;
        agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
        std::optional guard{test::work_tracking_executor(grpc_context)};
        grpc::Alarm alarm;
        asio::post(grpc_context,
                   [&]
                   {
                       agrpc::wait(alarm, test::five_seconds_from_now(),
                                   asio::bind_executor(grpc_context,
                                                       [&](bool)
                                                       {
                                                           ok = true;
                                                       }));
                       if (is_stop_from_same_thread)
                       {
                           grpc_context.stop();
                           guard.reset();
                       }
                       else
                       {
                           thread = std::thread(
                               [&]
                               {
                                   grpc_context.stop();
                                   guard.reset();
                               });
                       }
                   });
        grpc_context.run();
        CHECK_FALSE(ok);
        if (!is_stop_from_same_thread)
        {
            thread.join();
        }
    }
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::spawn an Alarm and yield its wait")
{
    bool ok{false};
    std::chrono::system_clock::time_point start;
    asio::spawn(asio::bind_executor(get_executor(), [] {}),
                [&](auto&& yield)
                {
                    grpc::Alarm alarm;
                    start = std::chrono::system_clock::now();
                    ok = agrpc::wait(alarm, test::hundred_milliseconds_from_now(), yield);
                });
    grpc_context.run();
    CHECK_LE(std::chrono::milliseconds(100), std::chrono::system_clock::now() - start);
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post an Alarm and check time")
{
    bool ok{false};
    std::chrono::system_clock::time_point start;
    grpc::Alarm alarm;
    asio::post(grpc_context,
               [&]()
               {
                   start = std::chrono::system_clock::now();
                   agrpc::wait(alarm, test::hundred_milliseconds_from_now(),
                               asio::bind_executor(grpc_context,
                                                   [&](bool)
                                                   {
                                                       ok = true;
                                                   }));
               });
    grpc_context.run();
    CHECK_LE(std::chrono::milliseconds(100), std::chrono::system_clock::now() - start);
    CHECK(ok);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "experimental::deferred with Alarm")
{
    bool ok1{false};
    bool ok2{false};
    grpc::Alarm alarm;
    auto deferred_op =
        agrpc::wait(alarm, test::ten_milliseconds_from_now(),
                    asio::experimental::deferred(
                        [&](bool wait_ok)
                        {
                            ok1 = wait_ok;
                            return agrpc::wait(alarm, test::ten_milliseconds_from_now(), asio::experimental::deferred);
                        }));
    std::move(deferred_op)(asio::bind_executor(grpc_context,
                                               [&](bool wait_ok)
                                               {
                                                   ok2 = wait_ok;
                                               }));
    grpc_context.run();
    CHECK(ok1);
    CHECK(ok2);
}
#endif

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post a asio::steady_timer")
{
    std::optional<test::ErrorCode> error_code;
    asio::steady_timer timer{get_executor()};
    asio::post(get_executor(),
               [&]
               {
                   timer.expires_after(std::chrono::milliseconds(10));
                   timer.async_wait(
                       [&](const test::ErrorCode& ec)
                       {
                           error_code.emplace(ec);
                       });
               });
    grpc_context.run();
    CHECK_EQ(test::ErrorCode{}, error_code);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::spawn with yield_context")
{
    bool ok{false};
    test::spawn_and_run(grpc_context,
                        [&](asio::yield_context yield)
                        {
                            grpc::Alarm alarm;
                            ok = agrpc::wait(alarm, test::ten_milliseconds_from_now(), yield);
                        });
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "post from multiple threads")
{
    static constexpr auto THREAD_COUNT = 32;
    std::atomic_int counter{};
    asio::thread_pool pool{THREAD_COUNT};
    std::optional guard{test::work_tracking_executor(grpc_context)};
    for (size_t i = 0; i < THREAD_COUNT; ++i)
    {
        asio::post(pool,
                   [&]
                   {
                       asio::post(grpc_context,
                                  [&]
                                  {
                                      if (++counter == THREAD_COUNT)
                                      {
                                          guard.reset();
                                      }
                                  });
                   });
    }
    asio::post(pool,
               [&]
               {
                   grpc_context.run();
               });
    pool.join();
    CHECK_EQ(THREAD_COUNT, counter);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "post/execute with allocator")
{
    SUBCASE("asio::post")
    {
        asio::post(grpc_context,
                   test::HandlerWithAssociatedAllocator{
                       [] {}, agrpc::detail::pmr::polymorphic_allocator<std::byte>(&resource)});
    }
    SUBCASE("asio::execute before grpc_context.run()")
    {
        get_pmr_executor().execute([] {});
    }
    SUBCASE("asio::execute after grpc_context.run() from same thread")
    {
        asio::post(grpc_context,
                   [&, exec = get_pmr_executor()]
                   {
                       exec.execute([] {});
                   });
    }
    SUBCASE("agrpc::wait")
    {
        asio::execution::execute(get_executor(),
                                 [&, executor = get_pmr_executor()]() mutable
                                 {
                                     auto alarm = std::make_shared<grpc::Alarm>();
                                     auto& alarm_ref = *alarm;
                                     agrpc::wait(alarm_ref, test::ten_milliseconds_from_now(),
                                                 asio::bind_executor(std::move(executor),
                                                                     [a = std::move(alarm)](bool ok)
                                                                     {
                                                                         CHECK(ok);
                                                                     }));
                                 });
    }
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "dispatch with allocator")
{
    asio::post(grpc_context,
               [&]
               {
                   asio::dispatch(get_pmr_executor(), [] {});
               });
    grpc_context.run();
    CHECK_FALSE(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "execute with throwing allocator")
{
    const auto executor =
        asio::require(get_executor(), asio::execution::allocator(agrpc::detail::pmr::polymorphic_allocator<std::byte>(
                                          agrpc::detail::pmr::null_memory_resource())));
    CHECK_THROWS(asio::execution::execute(executor, [] {}));
}

struct Exception : std::exception
{
};

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post with throwing completion handler")
{
    asio::post(get_executor(), asio::bind_executor(get_executor(),
                                                   []
                                                   {
                                                       throw Exception{};
                                                   }));
    CHECK_THROWS_AS(grpc_context.run(), Exception);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.poll() with asio::post")
{
    bool invoked{false};
    asio::io_context io_context;
    asio::post(io_context,
               [&]()
               {
                   CHECK_FALSE(grpc_context.poll());
                   asio::post(grpc_context,
                              [&]
                              {
                                  invoked = true;
                              });
                   CHECK_FALSE(invoked);
                   CHECK(grpc_context.poll());
               });
    io_context.run();
    CHECK(invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.poll() with grpc::Alarm")
{
    bool invoked{false};
    asio::io_context io_context;
    grpc::Alarm alarm;
    asio::steady_timer timer{io_context};
    asio::post(io_context,
               [&]()
               {
                   agrpc::wait(alarm, std::chrono::system_clock::now(),
                               asio::bind_executor(grpc_context,
                                                   [&](bool)
                                                   {
                                                       invoked = true;
                                                   }));
                   timer.expires_after(std::chrono::milliseconds(100));
                   timer.async_wait(
                       [&](auto&&)
                       {
                           CHECK_FALSE(invoked);
                           CHECK(grpc_context.poll());
                       });
               });
    io_context.run();
    CHECK(invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.poll_completion_queue()")
{
    bool post_completed{false};
    bool alarm_completed{false};
    asio::io_context io_context;
    grpc::Alarm alarm;
    asio::steady_timer timer{io_context};
    asio::post(io_context,
               [&]()
               {
                   asio::post(grpc_context,
                              [&]
                              {
                                  post_completed = true;
                              });
                   agrpc::wait(alarm, std::chrono::system_clock::now(),
                               asio::bind_executor(grpc_context,
                                                   [&](bool)
                                                   {
                                                       alarm_completed = true;
                                                   }));
                   timer.expires_after(std::chrono::milliseconds(100));
                   timer.async_wait(
                       [&](auto&&)
                       {
                           CHECK_FALSE(post_completed);
                           CHECK_FALSE(alarm_completed);
                           CHECK(grpc_context.poll_completion_queue());
                           CHECK_FALSE(post_completed);
                           CHECK(alarm_completed);
                           CHECK_FALSE(grpc_context.poll_completion_queue());
                           CHECK(grpc_context.poll());
                           CHECK(post_completed);
                       });
               });
    io_context.run();
}
}