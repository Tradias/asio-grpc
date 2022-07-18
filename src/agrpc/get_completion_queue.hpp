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

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

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
        return detail::query_grpc_context(executor).get_completion_queue();
    }

    /**
     * @brief Get `grpc::CompletionQueue*` from a GrpcContext
     *
     * Equivalent to `grpc_context.get_completion_queue()`
     */
    [[nodiscard]] inline grpc::CompletionQueue* operator()(agrpc::GrpcContext& grpc_context) const noexcept
    {
        return grpc_context.get_completion_queue();
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    /**
     * @brief Get `grpc::CompletionQueue*` from an `asio::any_io_executor`
     *
     * @attention *executor* must have been created from a GrpcExecutor
     */
    [[nodiscard]] inline grpc::CompletionQueue* operator()(const asio::any_io_executor& executor) const noexcept
    {
        return detail::query_grpc_context(executor).get_completion_queue();
    }

    /**
     * @brief Get `grpc::CompletionQueue*` from an object's associated executor
     *
     * First obtains the object's associated executor and then returns `agrpc::get_completion_queue(executor)`.
     *
     * @attention The associated executor must refer to a GrpcContext
     */
    template <class Object>
    [[nodiscard]] grpc::CompletionQueue* operator()(const Object& object) const noexcept
    {
        const auto executor = asio::get_associated_executor(object);
        return this->operator()(executor);
    }

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
    /**
     * @brief Get `grpc::CompletionQueue*` from the current awaitable
     *
     * First awaits `asio::this_coro::executor` and then returns `agrpc::get_completion_queue(executor)`.
     *
     * @attention The awaitable's executor must refer to a GrpcContext
     */
    template <class Executor = asio::any_io_executor>
    [[nodiscard]] auto operator()(asio::use_awaitable_t<Executor> = {}) const ->
        typename asio::async_result<asio::use_awaitable_t<Executor>, void(grpc::CompletionQueue*)>::return_type
    {
        const auto executor = co_await asio::this_coro::executor;
        co_return this->operator()(executor);
    }
#endif
#endif
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
