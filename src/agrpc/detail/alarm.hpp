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

#ifndef AGRPC_DETAIL_ALARM_HPP
#define AGRPC_DETAIL_ALARM_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/sender_implementation.hpp>
#include <agrpc/detail/wait.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Deadline, class Executor>
struct MoveAlarmSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Alarm = agrpc::BasicAlarm<Executor>;
    using Signature = void(bool, Alarm);
    using StopFunction = detail::AlarmCancellationFunction;

    struct Initiation
    {
        Alarm& alarm;
        Deadline deadline;
    };

    auto& stop_function_arg(const Initiation& initiation) noexcept
    {
        return alarm.emplace(static_cast<Alarm&&>(initiation.alarm)).alarm;
    }

    void initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation,
                  detail::TypeErasedGrpcTagOperation* operation)
    {
        Alarm* emplaced_alarm;
        if (alarm)
        {
            emplaced_alarm = &*alarm;
        }
        else
        {
            emplaced_alarm = &alarm.emplace(static_cast<Alarm&&>(initiation.alarm));
        }
        detail::AlarmInitFunction{emplaced_alarm->alarm, initiation.deadline}(grpc_context, operation);
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        on_done(ok, static_cast<Alarm&&>(*alarm));
    }

    std::optional<Alarm> alarm;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALARM_HPP
