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

#ifndef AGRPC_AGRPC_ASYNC_NOTIFY_WHEN_DONE_HPP
#define AGRPC_AGRPC_ASYNC_NOTIFY_WHEN_DONE_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/async_notify_when_done.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Server-side function to set notification for rpc completion
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * **Per-Operation Cancellation**
 *
 * None.
 */
struct AsyncNotfiyWhenDoneFn
{
    /**
     * @brief Set notification for rpc completion
     *
     * Has to be called before the rpc starts. Upon completion, `grpc::ServerContext::IsCancelled()` can be called to
     * check whether the rpc was cancelled.
     *
     * @note Due to https://github.com/grpc/grpc/issues/10136 there are work-tracking issues during server shutdown. See
     * below example for a workaround.
     *
     * Example:
     *
     * @snippet server.cpp async-notify-when-done-request-loop
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void()`.
     */
    template <class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context,
                    CompletionToken&& token = {}) const
        noexcept(std::is_same_v<agrpc::UseSender, detail::RemoveCrefT<CompletionToken>>)
    {
        return detail::async_initiate_sender_implementation<detail::AsyncNotfiyWhenDoneSenderImplementation>(
            grpc_context, {}, {server_context}, token);
    }
};
}  // namespace detail

/**
 * @brief Set notification for server-side rpc completion
 *
 * @link detail::AsyncNotfiyWhenDoneFn
 * Server-side function to set notification for rpc completion.
 * @endlink
 */
inline constexpr detail::AsyncNotfiyWhenDoneFn async_notify_when_done{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_ASYNC_NOTIFY_WHEN_DONE_HPP
