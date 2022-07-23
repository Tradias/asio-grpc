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

#ifndef AGRPC_AGRPC_GET_COMPLETION_QUEUE_HPP
#define AGRPC_AGRPC_GET_COMPLETION_QUEUE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Function object to get CompletionQueue from objects
 */
struct GetCompletionQueueFn
{
    /**
     * @brief Get `grpc::CompletionQueue*` from a BasicGrpcExecutor
     *
     * Effectively calls `executor.context().get_completion_queue()`
     */
    template <class Allocator, std::uint32_t Options>
    [[nodiscard]] grpc::CompletionQueue* operator()(
        const agrpc::BasicGrpcExecutor<Allocator, Options>& executor) const noexcept
    {
        return detail::get_completion_queue(executor);
    }

    /**
     * @brief Get `grpc::CompletionQueue*` from a GrpcContext
     *
     * Equivalent to `grpc_context.get_completion_queue()`
     */
    [[nodiscard]] grpc::CompletionQueue* operator()(agrpc::GrpcContext& grpc_context) const noexcept
    {
        return detail::get_completion_queue(grpc_context);
    }

    /**
     * @brief Get `grpc::CompletionQueue*` from a GrpcStream
     *
     * Effectively calls `agrpc::get_completion_queue(grpc_stream.get_executor())`
     *
     * @since 2.0.0
     */
    template <class Executor>
    [[nodiscard]] grpc::CompletionQueue* operator()(const agrpc::BasicGrpcStream<Executor>& grpc_stream) const noexcept
    {
        return detail::get_completion_queue(grpc_stream);
    }
};
}

/**
 * @brief Get `grpc::CompletionQueue*` from an object
 *
 * @link detail::GetCompletionQueueFn
 * Function to get CompletionQueue from objects.
 * @endlink
 */
inline constexpr detail::GetCompletionQueueFn get_completion_queue{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_GET_COMPLETION_QUEUE_HPP
