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

#ifndef AGRPC_AGRPC_INITIATE_HPP
#define AGRPC_AGRPC_INITIATE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/defaultCompletionToken.hpp"
#include "agrpc/detail/grpcInitiate.hpp"
#include "agrpc/detail/initiate.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"
#include "agrpc/grpcExecutor.hpp"

AGRPC_NAMESPACE_BEGIN()

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
/**
 * @brief `asio::awaitable` specialized on `agrpc::GrpcExecutor`
 */
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable_t` specialized on `agrpc::GrpcExecutor`
 */
using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable` specialized on `agrpc::GrpcExecutor`
 */
inline constexpr agrpc::GrpcUseAwaitable GRPC_USE_AWAITABLE{};

namespace pmr
{
/**
 * @brief `asio::awaitable` specialized on `agrpc::pmr::GrpcExecutor`
 */
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::pmr::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable_t` specialized on `agrpc::pmr::GrpcExecutor`
 */
using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::pmr::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable` specialized on `agrpc::pmr::GrpcExecutor`
 */
inline constexpr agrpc::pmr::GrpcUseAwaitable GRPC_USE_AWAITABLE{};
}  // namespace pmr
#endif

/**
 * @brief Default completion token for all asynchronous methods
 *
 * For Boost.Asio and standalone Asio: `asio::use_awaitable`
 *
 * For libunifex: `agrpc::use_sender`
 */
using DefaultCompletionToken = detail::DefaultCompletionToken;

/**
 * @brief Get `grpc::CompletionQueue*` from a GrpcExecutor
 *
 * Equivalent to `executor.context().get_completion_queue()`
 */
template <class Allocator, std::uint32_t Options>
[[nodiscard]] grpc::CompletionQueue* get_completion_queue(
    const agrpc::BasicGrpcExecutor<Allocator, Options>& executor) noexcept
{
    return executor.context().get_completion_queue();
}

/**
 * @brief Get `grpc::CompletionQueue*` from a GrpcContext
 *
 * Equivalent to `executor.context().get_completion_queue()`
 */
[[nodiscard]] inline grpc::CompletionQueue* get_completion_queue(agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.get_completion_queue();
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
/**
 * @brief Get `grpc::CompletionQueue*` from an `asio::any_io_executor`
 *
 * @attention `executor` must have been created from a GrpcExecutor
 */
[[nodiscard]] inline grpc::CompletionQueue* get_completion_queue(const asio::any_io_executor& executor) noexcept
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
[[nodiscard]] grpc::CompletionQueue* get_completion_queue(const Object& object) noexcept
{
    const auto executor = asio::get_associated_executor(object);
    return agrpc::get_completion_queue(executor);
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
/**
 * @brief Get `grpc::CompletionQueue*` from an object's associated executor
 *
 * First obtains `asio::this_coro::executor` and then returns `agrpc::get_completion_queue(executor)`.
 *
 * @attention The awaitable's executor must refer to a GrpcContext
 */
template <class Executor = asio::any_io_executor>
[[nodiscard]] auto get_completion_queue(asio::use_awaitable_t<Executor> = {}) ->
    typename asio::async_result<asio::use_awaitable_t<Executor>, void(grpc::CompletionQueue*)>::return_type
{
    const auto executor = co_await asio::this_coro::executor;
    co_return agrpc::get_completion_queue(executor);
}
#endif
#endif

namespace detail
{
struct GrpcInitiateFn
{
    template <class InitiatingFunction, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(InitiatingFunction initiating_function, CompletionToken token = {}) const
    {
        return detail::grpc_initiate(std::move(initiating_function), std::move(token));
    }
};
}  // namespace detail

inline constexpr detail::GrpcInitiateFn grpc_initiate{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_INITIATE_HPP
