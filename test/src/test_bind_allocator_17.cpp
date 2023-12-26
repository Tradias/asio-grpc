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
#include "utils/tracking_allocator.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/detail/bind_allocator.hpp>

#include <optional>

TEST_CASE_FIXTURE(test::GrpcContextTest, "AllocatorBinder constructor and member function")
{
    test::TrackingAllocator<> default_allocator{};

    agrpc::detail::AllocatorBinder int_binder{get_allocator(), 1};
    CHECK_EQ(1, int_binder.get());
    CHECK(std::is_same_v<const int&, decltype(std::as_const(int_binder).get())>);
    CHECK_EQ(get_allocator(), int_binder.get_allocator());

    agrpc::detail::AllocatorBinder<unsigned int, decltype(default_allocator)> uint_binder{default_allocator,
                                                                                          int_binder};
    CHECK_EQ(1.f, uint_binder.get());
    CHECK_EQ(default_allocator, uint_binder.get_allocator());
    agrpc::detail::AllocatorBinder<double, decltype(default_allocator)> double_binder{default_allocator,
                                                                                      std::as_const(int_binder)};
    CHECK_EQ(1.0, double_binder.get());
    CHECK_EQ(default_allocator, double_binder.get_allocator());
    agrpc::detail::AllocatorBinder<long, decltype(default_allocator)> long_binder{default_allocator,
                                                                                  std::move(int_binder)};
    CHECK_EQ(1L, long_binder.get());
    CHECK_EQ(default_allocator, long_binder.get_allocator());
    agrpc::detail::AllocatorBinder<long, decltype(default_allocator)> long_binder2{uint_binder};
    CHECK_EQ(1L, long_binder2.get());
    agrpc::detail::AllocatorBinder<long, decltype(default_allocator)> long_binder3{std::move(uint_binder)};
    CHECK_EQ(1L, long_binder3.get());

    bool invoked{false};
    auto allocator_binder = agrpc::detail::bind_allocator(default_allocator, asio::bind_executor(get_executor(),
                                                                                                 [&](bool ok)
                                                                                                 {
                                                                                                     invoked = ok;
                                                                                                 }));
    CHECK_EQ(get_executor(), asio::get_associated_executor(allocator_binder));
    allocator_binder(true);
    CHECK(invoked);
    std::as_const(allocator_binder)(false);
    CHECK_FALSE(invoked);

    struct MoveInvocable
    {
        constexpr bool operator()(bool ok) && { return ok; }
    };
    auto move_invocable_binder = agrpc::detail::bind_allocator(default_allocator, MoveInvocable{});
    CHECK(std::move(move_invocable_binder)(true));

    static constexpr auto ALLOCATOR_BINDER = agrpc::detail::bind_allocator(test::TrackingAllocator{},
                                                                           []
                                                                           {
                                                                               return 42;
                                                                           });
    static constexpr auto ALLOCATOR_BINDER_INVOKE_RESULT = ALLOCATOR_BINDER();
    CHECK_EQ(42, ALLOCATOR_BINDER_INVOKE_RESULT);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_allocator with old async_completion")
{
    auto completion_token = agrpc::detail::bind_allocator(get_allocator(), test::NoOp{});
    std::optional<decltype(get_allocator())> actual_allocator{};
    test::initiate_using_async_completion<decltype(completion_token), void()>(
        [&](auto&& ch)
        {
            actual_allocator.emplace(asio::get_associated_allocator(ch));
        },
        completion_token);
    CHECK_EQ(get_allocator(), actual_allocator);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_allocator with yield_context")
{
    test::spawn_and_run(grpc_context,
                        [&](const asio::yield_context& yield)
                        {
                            agrpc::Alarm alarm{grpc_context};
                            alarm.wait(test::ten_milliseconds_from_now(),
                                       agrpc::detail::bind_allocator(get_allocator(), yield));
                        });
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "bind_allocator with asio::steady_timer")
{
    asio::io_context io_context;
    asio::steady_timer timer{io_context};
    timer.async_wait(agrpc::detail::bind_allocator(get_allocator(), [](auto&&) {}));
    io_context.run();
    CHECK(allocator_has_been_used());
}