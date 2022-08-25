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
#include <agrpc/detail/executor_with_default.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/use_sender.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Sender completion token
 *
 * This function object can be used to create completion tokens that cause free functions in this library to return a
 * [sender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept). This is
 * particularly useful for libunifex where senders are also awaitable:
 *
 * @snippet unifex_client.cpp unifex-server-streaming-client-side
 *
 * For member functions in this library the `agrpc::UseSender` object must be used directly:
 * @code{cpp}
 * agrpc::RPC<...>::request(..., agrpc::use_sender);
 * @endcode
 */
struct UseSender
{
    /**
     * @brief Type alias to adapt an I/O object to use `agrpc::UseSender` as its default completion token type
     *
     * Only applicable to I/O objects of this library.
     */
    template <class T>
    using as_default_on_t =
        typename T::template rebind_executor<detail::ExecutorWithDefault<UseSender, typename T::executor_type>>::other;

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
 * @brief Instance and factory for sender completion tokens
 *
 * @link agrpc::UseSender
 * Sender completion token.
 * @endlink
 */
inline constexpr agrpc::UseSender use_sender{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_USE_SENDER_HPP
