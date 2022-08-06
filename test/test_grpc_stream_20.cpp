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

#include "utils/asio_utils.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/time.hpp"

#include <agrpc/cancel_safe.hpp>
#include <agrpc/grpc_stream.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>

#if defined(AGRPC_ASIO_HAS_CANCELLATION_SLOT) && defined(AGRPC_ASIO_HAS_CO_AWAIT)
TEST_CASE_FIXTURE(test::GrpcContextTest, "CancelSafe: co_await for a CancelSafe and an alarm parallel_group")
{
    test::co_spawn_and_run(grpc_context,
                           [&]() -> asio::awaitable<void>
                           {
                               agrpc::GrpcCancelSafe safe;
                               grpc::Alarm alarm1;
                               agrpc::wait(alarm1, test::five_hundred_milliseconds_from_now(),
                                           asio::bind_executor(grpc_context, safe.token()));
                               grpc::Alarm alarm2;
                               for (size_t i = 0; i < 3; ++i)
                               {
                                   auto [completion_order, alarm2_ok, alarm1_ec, alarm1_ok] =
                                       co_await asio::experimental::make_parallel_group(
                                           agrpc::wait(alarm2, test::ten_milliseconds_from_now(),
                                                       asio::bind_executor(grpc_context, asio::experimental::deferred)),
                                           safe.wait(asio::experimental::deferred))
                                           .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
                                   CHECK_EQ(0, completion_order[0]);
                                   CHECK_EQ(1, completion_order[1]);
                                   CHECK(alarm2_ok);
                                   CHECK_EQ(asio::error::operation_aborted, alarm1_ec);
                                   CHECK_EQ(bool{}, alarm1_ok);
                               }
                               CHECK(co_await safe.wait(agrpc::DefaultCompletionToken{}));
                           });
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcStream: next can be interrupted without cancelling initiated operation")
{
    test::co_spawn_and_run(grpc_context,
                           [&]() -> asio::awaitable<void>
                           {
                               agrpc::GrpcStream stream{grpc_context};
                               grpc::Alarm alarm;
                               stream.initiate(agrpc::wait, alarm, test::hundred_milliseconds_from_now());
                               grpc::Alarm alarm2;
                               using namespace asio::experimental::awaitable_operators;
                               auto result =
                                   co_await (agrpc::wait(alarm2, test::ten_milliseconds_from_now()) || stream.next());
                               CHECK_EQ(0, result.index());
                               CHECK(co_await stream.next());
                               co_await stream.cleanup();
                           });
}
#endif
