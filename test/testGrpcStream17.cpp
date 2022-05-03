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

#include "test/v1/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/time.hpp"

#include <agrpc/cancelSafe.hpp>
#include <agrpc/grpcStream.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcContextTest,
                  "GrpcStream: calling cleanup on a newly constructed stream completes immediately")
{
    bool invoked{};
    agrpc::GrpcStream stream{grpc_context};
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
    alarm.Cancel();
    stream.next(asio::bind_executor(grpc_context,
                                    [&](auto&& ec, bool ok)
                                    {
                                        CHECK_FALSE(ec);
                                        CHECK_FALSE(ok);
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
