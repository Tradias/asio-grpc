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

#ifndef AGRPC_AGRPC_WAIT_HPP
#define AGRPC_AGRPC_WAIT_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_initiate.hpp>
#include <agrpc/detail/wait.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Function object to wait for timers
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * All. Effectively calls
 * [grpc::Alarm::Cancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html#a57837c6b6d75f622c056b3050cf000fb)
 * which will cause the operation to complete with `false`.
 */
struct WaitFn
{
    /**
     * @brief Wait for a `grpc::Alarm`
     *
     * The operation finishes once the alarm expires (at deadline) or is cancelled (see
     * [Cancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html#a57837c6b6d75f622c056b3050cf000fb)). If the
     * alarm expired, the result will be true, false otherwise (ie, upon cancellation).
     *
     * Example:
     *
     * @snippet server.cpp alarm-awaitable
     *
     * @param deadline By default gRPC supports two types of deadlines: `gpr_timespec` and
     * `std::chrono::system_clock::time_point`. More types can be added by specializing
     * [grpc::TimePoint](https://grpc.github.io/grpc/cpp/classgrpc_1_1_time_point.html).
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` if it expired, `false` if it was canceled.
     */
    template <class Deadline, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::Alarm& alarm, const Deadline& deadline, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>&&
                     std::is_nothrow_copy_constructible_v<Deadline>)
    {
        return detail::grpc_initiate_impl<detail::AlarmCancellationFunction>(detail::AlarmInitFunction{alarm, deadline},
                                                                             static_cast<CompletionToken&&>(token));
    }

    template <class Executor, class Deadline, class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    decltype(auto) operator()(agrpc::BasicAlarm<Executor>& alarm, const Deadline& deadline,
                              CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{}) const
        noexcept(noexcept(alarm.wait(deadline, static_cast<CompletionToken&&>(token))))
    {
        return alarm.wait(deadline, static_cast<CompletionToken&&>(token));
    }
};
}

/**
 * @brief Wait for a timer
 *
 * @link detail::WaitFn
 * Function to wait for timers.
 * @endlink
 */
inline constexpr detail::WaitFn wait{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_WAIT_HPP
