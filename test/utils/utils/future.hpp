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

#ifndef AGRPC_UTILS_FUTURE_HPP
#define AGRPC_UTILS_FUTURE_HPP

#include <agrpc/alarm.hpp>
#include <agrpc/grpc_context.hpp>

#include <chrono>
#include <future>
#include <optional>

namespace test
{
template <class T, class Yield>
std::optional<std::conditional_t<std::is_same_v<void, T>, bool, T>> wait_for_future(agrpc::GrpcContext& grpc_context,
                                                                                    std::future<T>& future,
                                                                                    const Yield& yield)
{
    agrpc::Alarm alarm{grpc_context};
    for (int i{}; i < 50; ++i)
    {
        alarm.wait(std::chrono::system_clock::now() + std::chrono::milliseconds(10), yield);
        if (future.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
        {
            if constexpr (std::is_same_v<void, T>)
            {
                future.get();
                return false;
            }
            else
            {
                return future.get();
            }
        }
    }
    return std::nullopt;
}
}  // namespace test

#endif  // AGRPC_UTILS_FUTURE_HPP
