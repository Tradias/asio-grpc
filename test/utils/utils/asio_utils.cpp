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

#include <agrpc/wait.hpp>

namespace test
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
void spawn(agrpc::GrpcContext& grpc_context, const std::function<void(const asio::yield_context&)>& function)
{
    asio::spawn(grpc_context, function);
}

void wait(grpc::Alarm& alarm, std::chrono::system_clock::time_point deadline,
          const asio::executor_binder<std::function<void(bool)>, agrpc::GrpcExecutor>& function)
{
    agrpc::wait(alarm, deadline, function);
}

void post(agrpc::GrpcContext& grpc_context, const std::function<void()>& function)
{
    asio::post(grpc_context, function);
}

void post(const agrpc::GrpcExecutor& executor, const std::function<void()>& function)
{
    asio::post(executor, function);
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
void co_spawn(agrpc::GrpcContext& grpc_context, const std::function<asio::awaitable<void>()>& function,
              test::RethrowFirstArg rethrow)
{
    asio::co_spawn(grpc_context, function, rethrow);
}
#endif
#endif
}  // namespace test
