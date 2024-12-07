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

namespace test
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
void wait(agrpc::Alarm& alarm, std::chrono::system_clock::time_point deadline,
          const std::function<void(bool)>& function)
{
    alarm.wait(deadline, function);
}

void spawn(agrpc::GrpcContext& grpc_context, const std::function<void(const asio::yield_context&)>& function)
{
    test::typed_spawn(grpc_context, function);
}

void spawn(asio::io_context& io_context, const std::function<void(const asio::yield_context&)>& function)
{
    test::typed_spawn(io_context, function);
}

void post(agrpc::GrpcContext& grpc_context, const std::function<void()>& function)
{
    asio::post(grpc_context, function);
}

void post(const agrpc::GrpcExecutor& executor, const std::function<void()>& function)
{
    asio::post(executor, function);
}

#ifdef AGRPC_TEST_ASIO_HAS_CO_AWAIT
void co_spawn(agrpc::GrpcContext& grpc_context, const std::function<asio::awaitable<void>()>& function)
{
    asio::co_spawn(grpc_context, function, test::RethrowFirstArg{});
}
#endif
#endif
}  // namespace test
