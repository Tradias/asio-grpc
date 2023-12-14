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

#ifndef AGRPC_AGRPC_ALARM_HPP
#define AGRPC_AGRPC_ALARM_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/alarm.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/wait.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief I/O object for `grpc::Alarm`
 *
 * Wraps a [grpc::Alarm](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html) as an I/O object.
 *
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * All. Effectively calls
 * [grpc::Alarm::Cancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_alarm.html#a57837c6b6d75f622c056b3050cf000fb)
 * which will cause the operation to complete with `false`.
 *
 * @since 2.2.0
 */
template <class Executor>
class BasicAlarm
{
  public:
    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    /**
     * @brief Construct a BasicAlarm from an executor
     */
    explicit BasicAlarm(const Executor& executor) : executor_(executor) {}

    /**
     * @brief Construct a BasicAlarm from a GrpcContext
     */
    explicit BasicAlarm(agrpc::GrpcContext& grpc_context) : executor_(grpc_context.get_executor()) {}

    /**
     * @brief Wait until a specified deadline has been reached (lvalue overload)
     *
     * The operation finishes once the alarm expires (at deadline) or is cancelled. If the alarm expired, the result
     * will be true, false otherwise (i.e. upon cancellation).
     *
     * @attention Only one wait may be outstanding at a time.
     *
     * Example:
     *
     * @snippet server.cpp alarm-io-object-lvalue
     *
     * @param deadline By default gRPC supports two types of deadlines: `gpr_timespec` and
     * `std::chrono::system_clock::time_point`. More types can be added by specializing
     * [grpc::TimePoint](https://grpc.github.io/grpc/cpp/classgrpc_1_1_time_point.html).
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` if it expired, `false` if it was canceled.
     */
    template <class Deadline, class CompletionToken = detail::LegacyDefaultCompletionTokenT<Executor>>
    auto wait(const Deadline& deadline, CompletionToken&& token = detail::LegacyDefaultCompletionTokenT<Executor>{}) &
    {
        using Initiation = detail::GrpcSenderInitiation<detail::AlarmInitFunction<Deadline>>;
        if constexpr (std::is_same_v<agrpc::UseSender, detail::RemoveCrefT<CompletionToken>>)
        {
            return detail::BasicSenderAccess::create(grpc_context(), Initiation{alarm_, deadline},
                                                     detail::SenderAlarmSenderImplementation{});
        }
        else
        {
            return detail::async_initiate_sender_implementation(
                grpc_context(), Initiation{alarm_, deadline},
                detail::GrpcSenderImplementation<detail::AlarmCancellationFunction>{},
                static_cast<CompletionToken&&>(token));
        }
    }

    /**
     * @brief Wait until a specified deadline has been reached (rvalue overload)
     *
     * Extends the lifetime of the Alarm until the end of the wait. Otherwise, equivalent to the lvalue overload.
     *
     * Example:
     *
     * @snippet server.cpp alarm-io-object-rvalue
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool, BasicAlarm)`. `true` if it expired, `false` if it was canceled.
     */
    template <class Deadline, class CompletionToken = detail::LegacyDefaultCompletionTokenT<Executor>>
    auto wait(const Deadline& deadline, CompletionToken&& token = detail::LegacyDefaultCompletionTokenT<Executor>{}) &&
    {
        using Initiation = detail::MoveAlarmSenderInitiation<Deadline>;
        if constexpr (std::is_same_v<agrpc::UseSender, detail::RemoveCrefT<CompletionToken>>)
        {
            return detail::BasicSenderAccess::create(
                grpc_context(), Initiation{deadline},
                detail::SenderMoveAlarmSenderImplementation<Executor>{static_cast<BasicAlarm&&>(*this)});
        }
        else
        {
            return detail::async_initiate_sender_implementation(
                grpc_context(), Initiation{deadline},
                detail::MoveAlarmSenderImplementation<Executor>{static_cast<BasicAlarm&&>(*this)},
                static_cast<CompletionToken&&>(token));
        }
    }

    /**
     * @brief Cancel an outstanding wait
     *
     * The outstanding wait will complete with `false` if the Alarm did not fire yet, otherwise this function has no
     * effect.
     *
     * Thread-safe
     */
    void cancel() { alarm_.Cancel(); }

    /**
     * @brief Get the executor
     *
     * Thread-safe
     */
    [[nodiscard]] const executor_type& get_executor() const noexcept { return executor_; }

    /**
     * @brief Get the scheduler
     *
     * Thread-safe
     *
     * @since 2.9.0
     */
    [[nodiscard]] const executor_type& get_scheduler() const noexcept { return executor_; }

  private:
    template <class>
    friend struct detail::MoveAlarmSenderImplementation;

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor_); }

    Executor executor_;
    grpc::Alarm alarm_;
};

template <class = void>
BasicAlarm(agrpc::GrpcContext&) -> BasicAlarm<agrpc::GrpcExecutor>;

/**
 * @brief A BasicAlarm that uses `agrpc::GrpcExecutor`
 *
 * @since 2.2.0
 */
using Alarm = agrpc::BasicAlarm<agrpc::GrpcExecutor>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_ALARM_HPP
