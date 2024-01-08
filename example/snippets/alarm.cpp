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

#include "helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>

namespace asio = boost::asio;

asio::awaitable<void> grpc_alarm()
{
    /* [alarm-awaitable] */
    grpc::Alarm alarm;
    bool wait_ok =
        co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(1), asio::use_awaitable);
    /* [alarm-awaitable] */

    silence_unused(wait_ok);
}

asio::awaitable<void> agrpc_alarm_lvalue(agrpc::GrpcContext& grpc_context)
{
    /* [alarm-io-object-lvalue] */
    agrpc::Alarm alarm{grpc_context};
    bool wait_ok = co_await alarm.wait(std::chrono::system_clock::now() + std::chrono::seconds(1), asio::use_awaitable);
    /* [alarm-io-object-lvalue] */

    silence_unused(wait_ok);
}

asio::awaitable<void> agrpc_alarm_rvalue(agrpc::GrpcContext& grpc_context)
{
    /* [alarm-io-object-rvalue] */
    auto [alarm, wait_ok] = co_await agrpc::Alarm(grpc_context)
                                .wait(std::chrono::system_clock::now() + std::chrono::seconds(1), asio::use_awaitable);
    /* [alarm-io-object-rvalue] */

    silence_unused(alarm, wait_ok);
}

asio::awaitable<void> timer_with_different_completion_tokens(agrpc::GrpcContext& grpc_context,
                                                             asio::io_context& io_context)
{
    std::allocator<void> my_allocator{};
    agrpc::Alarm alarm{grpc_context};
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
    /* [alarm-with-callback] */
    alarm.wait(deadline, [&](bool /*wait_ok*/) {});
    /* [alarm-with-callback] */

    /* [alarm-with-spawn] */
    asio::spawn(
        io_context,
        [&](const asio::yield_context& yield)
        {
            agrpc::Alarm alarm{grpc_context};
            alarm.wait(deadline, yield);  // suspend coroutine until alarm fires
        },
        asio::detached);
    /* [alarm-with-spawn] */

    /* [alarm-with-allocator-aware-awaitable] */
    co_await alarm.wait(deadline, asio::bind_allocator(my_allocator, asio::use_awaitable));
    /* [alarm-with-allocator-aware-awaitable] */
}

// Explicitly formatted using `ColumnLimit: 90`
// clang-format off
/* [agrpc-alarm] */
asio::awaitable<void> agrpc_alarm(agrpc::GrpcContext& grpc_context)
{
    agrpc::Alarm alarm{grpc_context};
    bool wait_ok = co_await alarm.wait(
        std::chrono::system_clock::now() + std::chrono::seconds(1), asio::use_awaitable);
    (void)wait_ok;
}
/* [agrpc-alarm] */
// clang-format on
