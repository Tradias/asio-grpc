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

#ifndef AGRPC_AGRPC_NOTIFY_WHEN_DONE_HPP
#define AGRPC_AGRPC_NOTIFY_WHEN_DONE_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/notify_when_done.hpp>
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
 *
 * @since 2.3.0
 */
struct NotifyWhenDoneFn
{
    /**
     * @brief Set notification for rpc completion
     *
     * Has to be called before the rpc starts. Upon completion, `grpc::ServerContext::IsCancelled()` can be called to
     * check whether the rpc was cancelled.
     *
     * @attention This function does not work with `GrpcContext::run_completion_queue/poll_completion_queue()`. Use
     * `GrpcContext::run/poll()` instead.
     *
     * @note Due to https://github.com/grpc/grpc/issues/10136 there are work-tracking issues during server shutdown. See
     * below example for a workaround.
     *
     * Example:
     *
     * @snippet server.cpp notify-when-done-request-loop
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void()`.
     */
    template <class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context,
                    CompletionToken&& token = {}) const noexcept(detail::IS_USE_SENDER<CompletionToken>)
    {
        return detail::async_initiate_sender_implementation(grpc_context, detail::NotifyWhenDoneSenderInitiation{},
                                                            detail::NotifyWhenDoneSenderImplementation{server_context},
                                                            static_cast<CompletionToken&&>(token));
    }
};
}  // namespace detail

/**
 * @brief Set notification for server-side rpc completion
 *
 * @link detail::NotifyWhenDoneFn
 * Server-side function to set notification for rpc completion.
 * @endlink
 *
 * @since 2.3.0
 */
inline constexpr detail::NotifyWhenDoneFn notify_when_done{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_NOTIFY_WHEN_DONE_HPP
