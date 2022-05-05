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

#include "utils/grpcContextTest.hpp"
#include "utils/time.hpp"

#include <agrpc/cancelSafe.hpp>
#include <agrpc/grpcStream.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcContextTest, "CancelSafe: cancel wait for alarm and wait again")
{
    bool done{};
    agrpc::GrpcCancelSafe safe;
    grpc::Alarm alarm;
    agrpc::wait(alarm, test::five_hundred_milliseconds_from_now(), asio::bind_executor(grpc_context, safe.token()));
    asio::cancellation_signal signal;
    safe.wait(agrpc::bind_allocator(get_allocator(),
                                    asio::bind_cancellation_slot(signal.slot(), asio::bind_executor(grpc_context,
                                                                                                    [&](auto&& ec, bool)
                                                                                                    {
                                                                                                        done = !ec;
                                                                                                    }))));
    signal.emit(asio::cancellation_type::terminal);
    safe.wait(asio::bind_executor(grpc_context,
                                  [&](auto&&, bool)
                                  {
                                      CHECK_FALSE(done);
                                      done = true;
                                  }));
    grpc_context.run();
    CHECK(done);
    CHECK(allocator_has_been_used());
}

TEST_CASE_TEMPLATE("CancelSafe: wait before initiate", T, bool, test::ErrorCode)
{
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    bool ok{};
    agrpc::CancelSafe<T> safe;
    safe.wait(asio::bind_executor(grpc_context,
                                  [&](test::ErrorCode ec, auto&&...)
                                  {
                                      ok = !ec;
                                  }));
    safe.token()(T{});
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_TEMPLATE("CancelSafe: wait for already completed operation", T, bool, test::ErrorCode)
{
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    bool ok{};
    agrpc::CancelSafe<T> safe;
    safe.token()(T{});
    grpc::Alarm alarm;
    agrpc::wait(alarm, test::ten_milliseconds_from_now(),
                asio::bind_executor(grpc_context,
                                    [&](bool)
                                    {
                                        safe.wait(asio::bind_executor(grpc_context,
                                                                      [&](test::ErrorCode ec, auto&&...)
                                                                      {
                                                                          ok = !ec;
                                                                      }));
                                    }));
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE("CancelSafe: wait for asio::steady_timer")
{
    asio::io_context io_context;
    agrpc::CancelSafe<boost::system::error_code> safe;
    asio::steady_timer timer{io_context, std::chrono::seconds(5)};
    timer.async_wait(safe.token());
    asio::cancellation_signal signal;
    safe.wait(asio::bind_cancellation_slot(signal.slot(),
                                           asio::bind_executor(io_context,
                                                               [&](test::ErrorCode ec)
                                                               {
                                                                   CHECK_EQ(asio::error::operation_aborted, ec);
                                                                   CHECK_EQ(1, timer.cancel());
                                                               })));
    signal.emit(asio::cancellation_type::all);
    io_context.run();
}

TEST_CASE("CancelSafe: can handle move-only completion arguments")
{
    asio::io_context io_context;
    agrpc::CancelSafe<std::unique_ptr<int>> safe;
    auto token = safe.token();
    asio::async_initiate<decltype(token), void(std::unique_ptr<int> &&)>(
        [&](auto ch)
        {
            asio::post(io_context,
                       [&, ch = std::move(ch)]() mutable
                       {
                           std::move(ch)(std::make_unique<int>(42));
                       });
        },
        token);
    safe.wait(
        [&](test::ErrorCode ec, std::unique_ptr<int>&& actual)
        {
            CHECK_FALSE(ec);
            CHECK_EQ(42, *actual);
        });
    io_context.run();
}

TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "GrpcStream: calling cleanup on a newly constructed stream completes immediately")
{
    bool invoked{};
    agrpc::GrpcStream stream{grpc_context};
    CHECK_FALSE(stream.is_running());
    stream.cleanup(asio::bind_executor(grpc_context,
                                       [&](auto&&, bool)
                                       {
                                           invoked = true;
                                       }));
    grpc_context.run();
    CHECK(invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcStream: initiate alarm -> cancel alarm -> next returns false")
{
    agrpc::GrpcStream stream{grpc_context};
    grpc::Alarm alarm;
    stream.initiate(agrpc::wait, alarm, test::five_seconds_from_now());
    CHECK(stream.is_running());
    alarm.Cancel();
    stream.next(asio::bind_executor(grpc_context,
                                    [&](auto&& ec, bool ok)
                                    {
                                        CHECK_FALSE(ec);
                                        CHECK_FALSE(ok);
                                        CHECK_FALSE(stream.is_running());
                                        stream.cleanup([](auto&&, bool) {});
                                    }));
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcStream: initiate can customize allocator")
{
    agrpc::GrpcStream stream{grpc_context};
    grpc::Alarm alarm;
    stream.initiate(std::allocator_arg, get_allocator(), agrpc::wait, alarm, test::ten_milliseconds_from_now());
    stream.cleanup([](auto&&, bool) {});
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcStream: can change default completion token")
{
    static bool is_ok{};
    struct Callback
    {
        explicit Callback() {}

        void operator()(test::ErrorCode, bool ok) { is_ok = ok; }
    };
    struct Exec : agrpc::GrpcExecutor
    {
        using default_completion_token_type = Callback;

        explicit Exec(agrpc::GrpcExecutor exec) : agrpc::GrpcExecutor(exec) {}

        // Workaround for Asio misdetecting BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT in MSVC C++17
        auto& context() const noexcept { return this->query(asio::execution::context); }
    };
    agrpc::BasicGrpcStream<Exec> stream{grpc_context};
    grpc::Alarm alarm;
    stream.initiate(agrpc::wait, alarm, test::ten_milliseconds_from_now());
    stream.cleanup();
    grpc_context.run();
    CHECK(is_ok);
}
}
#endif
