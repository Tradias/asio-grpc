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

#ifndef AGRPC_AGRPC_USESENDER_HPP
#define AGRPC_AGRPC_USESENDER_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/queryGrpcContext.hpp"
#include "agrpc/detail/useSender.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct UseSchedulerFn
{
    template <class Scheduler>
    [[deprecated("renamed to use_sender")]] [[nodiscard]] auto operator()(const Scheduler& scheduler) const noexcept
    {
        return detail::UseSender{detail::query_grpc_context(scheduler)};
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    [[deprecated("renamed to use_sender")]] [[nodiscard]] auto operator()(
        asio::execution_context& context) const noexcept
    {
        return detail::UseSender{static_cast<agrpc::GrpcContext&>(context)};
    }
#endif

    [[deprecated("renamed to use_sender")]] [[nodiscard]] auto operator()(agrpc::GrpcContext& context) const noexcept
    {
        return detail::UseSender{context};
    }
};

/**
 * @brief Function object to create sender completion tokens
 *
 * The completion token created by this function causes other asynchronous functions in this library to return a
 * [Sender](https://brycelelbach.github.io/wg21_p2300_std_execution/std_execution.html#design-senders). This is
 * particularly useful for libunifex where senders are also awaitable:
 *
 * @snippet unifex-client.cpp unifex-server-streaming-client-side
 */
struct UseSenderFn
{
    /**
     * @brief Overload for BasicGrpcExecutor
     */
    template <class Allocator, std::uint32_t Options>
    [[nodiscard]] constexpr auto operator()(const agrpc::BasicGrpcExecutor<Allocator, Options>& executor) const noexcept
    {
        return detail::UseSender{executor.context()};
    }

    /**
     * @brief Overload for GrpcContext
     */
    [[nodiscard]] constexpr auto operator()(agrpc::GrpcContext& context) const noexcept
    {
        return detail::UseSender{context};
    }
};
}  // namespace detail

[[deprecated("renamed to use_sender")]] inline constexpr detail::UseSchedulerFn use_scheduler{};

/**
 * @brief Create sender completion token
 *
 * @link detail::UseSenderFn
 * Function to create sender completion tokens.
 * @endlink
 */
inline constexpr detail::UseSenderFn use_sender{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_USESENDER_HPP
