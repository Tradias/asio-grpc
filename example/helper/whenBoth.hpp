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

#ifndef AGRPC_HELPER_WHENBOTH_HPP
#define AGRPC_HELPER_WHENBOTH_HPP

#include <agrpc/useAwaitable.hpp>

#include <atomic>
#include <cstddef>
#include <optional>
#include <tuple>

namespace example
{
// Simple dynamic-allocation free implementation. Could also use asio::experimental::parallel_group but it
// performs one allocate_shared that cannot be customized.
template <class Result1, class Result2, class Init1, class Init2>
agrpc::GrpcAwaitable<std::pair<Result1, Result2>> when_both(Init1&& init1, Init2&& init2)
{
    std::optional<boost::asio::async_result<agrpc::GrpcUseAwaitable, void()>::handler_type> completion_handler;
    std::optional<Result1> result1;
    std::optional<Result2> result2;
    std::atomic_size_t count{};
    auto token = agrpc::GRPC_USE_AWAITABLE;
    co_await boost::asio::async_initiate<agrpc::GrpcUseAwaitable, void()>(
        [&](auto&& ch)
        {
            completion_handler.emplace(std::move(ch));
            const auto complete = [&]()
            {
                count.fetch_add(1, std::memory_order_relaxed);
                if (2 == count.load(std::memory_order_relaxed))
                {
                    // Make sure to destroy the completion_handler right after it has been invoked.
                    auto h = std::move(*completion_handler);
                    completion_handler.reset();
                    std::move(h)();
                }
            };
            init1(
                [&, complete](auto&&... result)
                {
                    result1.emplace(std::forward<decltype(result)>(result)...);
                    complete();
                });
            init2(
                [&, complete](auto&&... result)
                {
                    result2.emplace(std::forward<decltype(result)>(result)...);
                    complete();
                });
        },
        token);
    co_return std::pair{*std::move(result1), *std::move(result2)};
}
}

#endif  // AGRPC_HELPER_WHENBOTH_HPP
