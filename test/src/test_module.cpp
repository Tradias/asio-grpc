// Copyright 2026 Dennis Hezel
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

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <doctest/doctest.h>
#include <grpcpp/grpcpp.h>

#include <chrono>

import asio_grpc;

TEST_CASE("cpp 20 module")
{
    using GrpcAwaitable = boost::asio::awaitable<void, agrpc::GrpcExecutor>;
    constexpr boost::asio::use_awaitable_t<agrpc::GrpcExecutor> GRPC_USE_AWAITABLE{};
    bool ok1{false};
    agrpc::GrpcContext grpc_context;
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> GrpcAwaitable
        {
            agrpc::Alarm alarm{grpc_context};
            ok1 = co_await alarm.wait(std::chrono::system_clock::now() + std::chrono::seconds(1), GRPC_USE_AWAITABLE);
        },
        boost::asio::detached);
    grpc_context.run();
    CHECK(ok1);
}
