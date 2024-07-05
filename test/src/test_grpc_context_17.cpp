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
#include "utils/exception.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/io_context_test.hpp"
#include "utils/throwing_allocator.hpp"
#include "utils/time.hpp"
#include "utils/unassignable_allocator.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

#include <thread>

TEST_CASE("GrpcExecutor fulfills Executor TS traits")
{
    using Exec = agrpc::GrpcContext::executor_type;
    CHECK(asio::execution::is_executor_v<Exec>);
    CHECK(asio::can_require_v<Exec, asio::execution::blocking_t::never_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::blocking_t::possibly_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::relationship_t::fork_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::relationship_t::continuation_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::outstanding_work_t::tracked_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::outstanding_work_t::untracked_t>);
    CHECK(asio::can_prefer_v<Exec, asio::execution::allocator_t<test::TrackingAllocator<>>>);
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

struct GrpcExecutorTest : test::GrpcContextTest
{
    agrpc::GrpcContext other_grpc_context;

    auto other_executor() noexcept { return other_grpc_context.get_executor(); }

    auto other_work_tracking_executor() noexcept
    {
        return asio::require(other_executor(), asio::execution::outstanding_work_t::tracked);
    }
};

TEST_CASE_FIXTURE(GrpcExecutorTest, "Work tracking GrpcExecutor constructor and assignment")
{
    int this_marker{};
    int other_marker{};
    const auto this_executor = [&]
    {
        return asio::require(get_work_tracking_executor(),
                             asio::execution::allocator(test::UnassignableAllocator<std::byte>(&this_marker)));
    };
    const auto other_executor = [&]
    {
        return asio::require(other_work_tracking_executor(),
                             asio::execution::allocator(test::UnassignableAllocator<std::byte>(&other_marker)));
    };
    const auto has_work = [](agrpc::GrpcContext& context)
    {
        return !context.is_stopped();
    };
    const auto context = [](auto&& executor)
    {
        return std::addressof(asio::query(executor, asio::execution::context));
    };
    SUBCASE("copy construct")
    {
        std::optional ex1{this_executor()};
        CHECK(has_work(grpc_context));
        auto ex2{*ex1};
        CHECK_EQ(ex1, ex2);
        ex1.reset();
        CHECK(has_work(grpc_context));
    }
    SUBCASE("move construct")
    {
        auto ex1 = this_executor();
        {
            auto ex2{std::move(ex1)};
            CHECK_EQ(this_executor(), ex2);
            CHECK(has_work(grpc_context));
        }
        CHECK_FALSE(has_work(grpc_context));
    }
    SUBCASE("copy assign - same GrpcContext")
    {
        std::optional ex1{this_executor()};
        auto ex2 = this_executor();
        ex2 = std::as_const(*ex1);
        ex2 = std::as_const(ex2);
        CHECK_EQ(ex1, ex2);
        ex1.reset();
        CHECK(has_work(grpc_context));
    }
    SUBCASE("copy assign - other GrpcContext")
    {
        std::optional ex1{this_executor()};
        auto ex2 = other_executor();
        CHECK(has_work(other_grpc_context));
        ex2 = std::as_const(*ex1);
        CHECK_EQ(context(*ex1), context(ex2));
        CHECK_NE(ex1, ex2);
        ex1.reset();
        CHECK(has_work(grpc_context));
        CHECK_FALSE(has_work(other_grpc_context));
    }
    SUBCASE("move assign - same GrpcContext")
    {
        auto ex1 = this_executor();
        {
            auto ex2 = this_executor();
            ex2 = std::move(ex1);
            ex2 = std::move(ex2);
#ifdef _MSC_VER
#pragma warning(suppress : 26800)
#endif
            CHECK_EQ(this_executor(), ex2);
            CHECK(has_work(grpc_context));
        }
        CHECK_FALSE(has_work(grpc_context));
    }
    SUBCASE("move assign - other GrpcContext")
    {
        std::optional ex1{this_executor()};
        auto ex2 = other_executor();
        ex2 = std::move(*ex1);
        CHECK_EQ(context(this_executor()), context(ex2));
        CHECK_NE(this_executor(), ex2);
        ex1.reset();
        CHECK(has_work(grpc_context));
        CHECK_FALSE(has_work(other_grpc_context));
    }
    CHECK_FALSE(has_work(grpc_context));
}

TEST_CASE_FIXTURE(GrpcExecutorTest, "GrpcExecutor comparison operator - different options")
{
    CHECK_EQ(get_executor(), asio::require(get_executor(), asio::execution::blocking_t::never));
    CHECK_NE(get_executor(), asio::require(get_executor(), asio::execution::blocking_t::possibly));
    CHECK_NE(other_executor(), asio::require(get_executor(), asio::execution::blocking_t::never));
    CHECK_NE(other_executor(), asio::require(get_executor(), asio::execution::blocking_t::possibly));
}

TEST_CASE_FIXTURE(GrpcExecutorTest, "GrpcExecutor comparison operator - different allocator")
{
    CHECK_EQ(get_executor(), asio::require(get_executor(), asio::execution::allocator));
    auto alloc_executor = asio::require(get_executor(), asio::execution::allocator(test::TrackingAllocator<>()));
    auto other_alloc_executor =
        asio::require(other_executor(), asio::execution::allocator(test::TrackingAllocator<>()));
    SUBCASE("same options")
    {
        CHECK_EQ(alloc_executor, alloc_executor);
        CHECK_NE(alloc_executor, asio::require(alloc_executor, asio::execution::allocator(get_allocator())));
        CHECK_NE(other_alloc_executor, alloc_executor);
        CHECK_NE(other_alloc_executor, asio::require(alloc_executor, asio::execution::allocator(get_allocator())));
    }
    SUBCASE("different options")
    {
        CHECK_NE(alloc_executor, asio::require(alloc_executor, asio::execution::blocking_t::possibly));
        CHECK_NE(alloc_executor, asio::require(alloc_executor, asio::execution::blocking_t::possibly,
                                               asio::execution::allocator(get_allocator())));
        CHECK_NE(other_alloc_executor, asio::require(alloc_executor, asio::execution::blocking_t::possibly));
        CHECK_NE(other_alloc_executor, asio::require(alloc_executor, asio::execution::blocking_t::possibly,
                                                     asio::execution::allocator(get_allocator())));
    }
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext::reset")
{
    bool ok{false};
    CHECK_FALSE(grpc_context.is_stopped());
    post(
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
    post(
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
    post(
        [&]
        {
            grpc_context.stop();
            post(
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
        agrpc::Alarm alarm{grpc_context};
        test::post(grpc_context,
                   [&]
                   {
                       test::wait(alarm, test::five_seconds_from_now(),
                                  [&](bool)
                                  {
                                      ok = true;
                                  });
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

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post a asio::steady_timer")
{
    std::optional<test::ErrorCode> error_code;
    asio::steady_timer timer{get_executor()};
    test::post(get_executor(),
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
                       post(
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
        asio::post(grpc_context, test::HandlerWithAssociatedAllocator{test::NoOp{}, get_allocator()});
    }
    SUBCASE("asio::execute before grpc_context.run()") { get_tracking_allocator_executor().execute(test::NoOp{}); }
    SUBCASE("asio::execute after grpc_context.run() from same thread")
    {
        asio::post(grpc_context,
                   [&, exec = get_tracking_allocator_executor()]
                   {
                       exec.execute(test::NoOp{});
                   });
    }
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "post large local allocation")
{
    bool ok{};
    post(
        [&]
        {
            asio::post(grpc_context,
                       [&, a = std::array<char, 5000>{}]
                       {
                           (void)a;
                           ok = true;
                       });
        });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "dispatch with allocator")
{
    post(
        [&]
        {
            asio::dispatch(get_tracking_allocator_executor(), test::NoOp{});
        });
    grpc_context.run();
    CHECK_FALSE(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "execute with throwing allocator")
{
    const auto executor = asio::require(get_executor(), asio::execution::allocator(test::ThrowingAllocator<>{}));
    CHECK_THROWS(executor.execute(test::NoOp{}));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post with throwing completion handler")
{
    asio::post(get_executor(), asio::bind_executor(get_executor(),
                                                   []
                                                   {
                                                       throw test::Exception{};
                                                   }));
    CHECK_THROWS_AS(grpc_context.run(), test::Exception);
}

struct GrpcContextAndIoContextTest : test::GrpcContextTest, test::IoContextTest
{
};

TEST_CASE_FIXTURE(GrpcContextAndIoContextTest, "GrpcContext.poll() with asio::post")
{
    bool invoked{false};
    asio::steady_timer timer{io_context};
    asio::post(io_context,
               [&]()
               {
                   CHECK_FALSE(grpc_context.poll());
                   post(
                       [&]
                       {
                           invoked = true;
                       });
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

TEST_CASE_FIXTURE(GrpcContextAndIoContextTest, "GrpcContext.poll() with agrpc::Alarm")
{
    bool invoked{false};
    agrpc::Alarm alarm{grpc_context};
    asio::steady_timer timer{io_context};
    asio::post(io_context,
               [&]()
               {
                   test::wait(alarm, test::now(),
                              [&](bool)
                              {
                                  invoked = true;
                              });
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

TEST_CASE_FIXTURE(GrpcContextAndIoContextTest, "GrpcContext.poll_completion_queue()")
{
    bool post_completed{false};
    bool alarm_completed{false};
    agrpc::Alarm alarm{grpc_context};
    asio::steady_timer timer{io_context};
    asio::post(io_context,
               [&]()
               {
                   post(
                       [&]
                       {
                           post_completed = true;
                       });
                   test::wait(alarm, test::now(),
                              [&](bool)
                              {
                                  alarm_completed = true;
                              });
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

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.run_completion_queue()")
{
    bool post_completed{false};
    bool alarm_completed{false};
    agrpc::Alarm alarm{grpc_context};
    post(
        [&]
        {
            post_completed = true;
        });
    test::wait(alarm, test::hundred_milliseconds_from_now(),
               [&](bool)
               {
                   CHECK_FALSE(post_completed);
                   alarm_completed = true;
                   grpc_context.stop();
               });
    CHECK(grpc_context.run_completion_queue());
    CHECK_FALSE(post_completed);
    CHECK(grpc_context.run());
    CHECK(post_completed);
    CHECK_FALSE(grpc_context.run_completion_queue());
}

inline void wait_some(agrpc::GrpcContext& grpc_context)
{
    agrpc::Alarm{grpc_context}.wait(test::now(),
                                    [&](auto&&...)
                                    {
                                        wait_some(grpc_context);
                                    });
}

inline void post_some(agrpc::GrpcContext& grpc_context, asio::thread_pool& pool)
{
    asio::post(pool,
               [&]
               {
                   asio::post(grpc_context,
                              [&]
                              {
                                  post_some(grpc_context, pool);
                              });
               });
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.run() parallel")
{
    grpc_context_lifetime.emplace(5);
    for (size_t j{}; j != 3; ++j)
    {
        asio::thread_pool pool{4};
        for (size_t i{}; i != 2; ++i)
        {
            asio::post(pool,
                       [&]
                       {
                           wait_some(grpc_context);
                       });
        }
        for (size_t i{}; i != 2; ++i)
        {
            post_some(grpc_context, pool);
        }
        for (size_t i{}; i != 2; ++i)
        {
            asio::post(pool,
                       [&]
                       {
                           grpc_context.run();
                       });
        }
        asio::steady_timer timer{pool, std::chrono::milliseconds(500)};
        timer.async_wait(
            [&](auto&&...)
            {
                grpc_context.stop();
            });
        pool.wait();
    }
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.poll() within run()")
{
    int count{};
    post(
        [&]
        {
            post(
                [&]
                {
                    ++count;
                });
            CHECK(grpc_context.poll());
            CHECK_EQ(1, count);
            post(
                [&]
                {
                    ++count;
                });
        });
    grpc_context.run();
    CHECK_EQ(2, count);
}

void recursively_post(agrpc::GrpcContext& grpc_context)
{
    test::post(grpc_context,
               [&]
               {
                   recursively_post(grpc_context);
               });
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.run() is not blocked by repeated asio::posts")
{
    bool alarm_completed{false};
    recursively_post(grpc_context);
    agrpc::Alarm alarm{grpc_context};
    post(
        [&]
        {
            test::wait(alarm, test::now(),
                       [&](bool)
                       {
                           alarm_completed = true;
                           grpc_context.stop();
                       });
        });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.run_until() can wait for agrpc::Alarm")
{
    bool invoked{false};
    agrpc::Alarm alarm{grpc_context};
    test::wait(alarm, test::now(),
               [&](bool)
               {
                   invoked = true;
               });
    CHECK(grpc_context.run_until(test::one_second_from_now()));
    CHECK(grpc_context.is_stopped());
    CHECK(invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.run_until() times out correctly")
{
    agrpc::Alarm alarm{grpc_context};
    test::wait(alarm, test::one_second_from_now(), test::NoOp{});
    CHECK_FALSE(grpc_context.run_until(test::now()));
    CHECK_FALSE(grpc_context.run_until(test::ten_milliseconds_from_now()));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcContext.run_while() runs until the expected event")
{
    bool alarm1_finished{false};
    agrpc::Alarm alarm1{grpc_context};
    test::wait(alarm1, test::one_second_from_now(),
               [&](bool)
               {
                   alarm1_finished = true;
               });
    bool alarm2_finished{false};
    auto sync_api = [&]
    {
        agrpc::Alarm alarm2{grpc_context};
        test::wait(alarm2, test::ten_milliseconds_from_now(),
                   [&](bool)
                   {
                       alarm2_finished = true;
                   });
        grpc_context.run_while(
            [&]
            {
                return !alarm2_finished;
            });
    };
    post(
        [&]
        {
            sync_api();
            CHECK_FALSE(alarm1_finished);
            CHECK(alarm2_finished);
            alarm1.cancel();
        });
    CHECK(grpc_context.run());
    CHECK(alarm1_finished);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio GrpcExecutor::schedule")
{
    bool invoked{false};
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  invoked = true;
                                              },
                                              state};
    auto operation_state = get_executor().schedule().connect(receiver);
    operation_state.start();
    CHECK_FALSE(invoked);
    grpc_context.run();
    CHECK(invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE("asio GrpcExecutor::schedule and destruct GrpcContext")
{
    bool invoked{false};
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  invoked = true;
                                              },
                                              state};
    {
        std::optional<agrpc::GrpcContext> grpc_context{std::make_unique<grpc::CompletionQueue>()};
        auto sender = grpc_context->get_scheduler().schedule();
        auto operation_state = sender.connect(receiver);
        operation_state.start();
        grpc_context.reset();
    }
    CHECK_FALSE(invoked);
    CHECK_FALSE(state.exception);
    CHECK(state.was_done);
}