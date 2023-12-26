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

#ifndef AGRPC_AGRPC_NOTIFY_ON_STATE_CHANGE_HPP
#define AGRPC_AGRPC_NOTIFY_ON_STATE_CHANGE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/default_completion_token.hpp>
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
     * @param deadline By default gRPC supports two types of deadlines: `gpr_timespec` and
     * `std::chrono::system_clock::time_point`. More types can be added by specializing
     * [grpc::TimePoint](https://grpc.github.io/grpc/cpp/classgrpc_1_1_time_point.html).
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` if the state changed, `false` if the deadline expired.
     */
    template <class Deadline, class CompletionToken>
    auto operator()(agrpc::GrpcContext& grpc_context, grpc::ChannelInterface& channel,
                    ::grpc_connectivity_state last_observed, Deadline deadline, CompletionToken&& token) const
        noexcept(detail::IS_USE_SENDER<CompletionToken> && std::is_nothrow_copy_constructible_v<Deadline>)
    {
        return detail::async_initiate_sender_implementation(
            grpc_context,
            detail::GrpcSenderInitiation<detail::NotifyOnStateChangeInitFunction<Deadline>>{channel, deadline,
                                                                                            last_observed},
            detail::GrpcSenderImplementation{}, static_cast<CompletionToken&&>(token));
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
