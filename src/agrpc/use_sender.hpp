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

#ifndef AGRPC_AGRPC_USE_SENDER_HPP
#define AGRPC_AGRPC_USE_SENDER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/use_sender.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Function object to create sender completion tokens
 *
 * The completion token created by this function causes other asynchronous functions in this library to return a
 * [Sender](https://brycelelbach.github.io/wg21_p2300_std_execution/std_execution.html#design-senders). This is
 * particularly useful for libunifex where senders are also awaitable:
 *
 * @snippet unifex_client.cpp unifex-server-streaming-client-side
 */
struct UseSender
{
    /**
     * @brief Overload for BasicGrpcExecutor
     */
    template <class Allocator, std::uint32_t Options>
    [[nodiscard]] constexpr detail::UseSender operator()(
        const agrpc::BasicGrpcExecutor<Allocator, Options>& executor) const noexcept
    {
        return detail::UseSender{detail::query_grpc_context(executor)};
    }

    /**
     * @brief Overload for GrpcContext
     */
    [[nodiscard]] constexpr detail::UseSender operator()(agrpc::GrpcContext& context) const noexcept
    {
        return detail::UseSender{context};
    }
};

/**
 * @brief Create sender completion token
 *
 * @link detail::UseSenderFn
 * Function to create sender completion tokens.
 * @endlink
 */
inline constexpr agrpc::UseSender use_sender{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_USE_SENDER_HPP
