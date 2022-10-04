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
 * @brief Function object to wait for timers
 */
template <class Executor>
class BasicAlarm
{
  public:
    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    explicit BasicAlarm(const Executor& executor) : executor(executor) {}

    explicit BasicAlarm(agrpc::GrpcContext& grpc_context) : executor(grpc_context.get_executor()) {}

    template <class Deadline, class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait(const Deadline& deadline, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{}) &
    {
        return detail::async_initiate_sender_implementation<
            detail::GrpcSenderImplementation<detail::AlarmInitFunction<Deadline>, detail::AlarmCancellationFunction>>(
            grpc_context(), {alarm, deadline}, {}, token);
    }

    template <class Deadline, class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait(const Deadline& deadline, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{}) &&
    {
        return detail::async_initiate_sender_implementation<detail::MoveAlarmSenderImplementation<Deadline, Executor>>(
            grpc_context(), {*this, deadline}, {}, token);
    }

    /**
     * @brief Get the executor
     *
     * Thread-safe
     */
    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

  private:
    template <class, class>
    friend struct detail::MoveAlarmSenderImplementation;

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor); }

    Executor executor;
    grpc::Alarm alarm;

};

using Alarm = agrpc::BasicAlarm<agrpc::GrpcExecutor>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_ALARM_HPP
