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

#ifndef AGRPC_AGRPC_NOTIFY_ON_STATE_CHANGE_HPP
#define AGRPC_AGRPC_NOTIFY_ON_STATE_CHANGE_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/notify_on_state_change.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Function to set notification for a `grpc::Channel` state change
 *
 * Wait for the channel state to change or the specified deadline to expire.
 *
 * **Per-Operation Cancellation**
 *
 * None.
 *
 * @since 2.3.0
 */
struct NotifyOnStateChangeFn
{
    /**
     * @brief Set notification for a `grpc::Channel` state change
     *
     * Wait for the channel state to change or the specified deadline to expire.
     *
     * Example:
     *
     * @snippet client.cpp notify_on_state_change
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` if the state changed, `false` if the deadline expired.
     */
    template <class Deadline, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(agrpc::GrpcContext& grpc_context, grpc::ChannelInterface& channel,
                    ::grpc_connectivity_state last_observed, const Deadline& deadline,
                    CompletionToken&& token = {}) const
        noexcept(std::is_same_v<agrpc::UseSender, detail::RemoveCrefT<CompletionToken>>)
    {
        return detail::async_initiate_sender_implementation<
            detail::GrpcSenderImplementation<detail::NotifyOnStateChangeInitFunction>>(
            grpc_context, {channel, last_observed, grpc::TimePoint<Deadline>(deadline).raw_time()}, {}, token);
    }
};
}  // namespace detail

/**
 * @brief Set notification for a `grpc::Channel` state change
 *
 * @link detail::NotifyOnStateChangeFn
 * Function to set notification for a `grpc::Channel` state change.
 * @endlink
 *
 * @since 2.3.0
 */
inline constexpr detail::NotifyOnStateChangeFn notify_on_state_change{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_NOTIFY_ON_STATE_CHANGE_HPP
