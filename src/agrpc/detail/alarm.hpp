// Copyright 2025 Dennis Hezel
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

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/sender_implementation.hpp>

#include <agrpc/detail/asio_macros.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Deadline>
struct AlarmInitFunction
{
    grpc::Alarm& alarm_;
    Deadline deadline_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        alarm_.Set(grpc_context.get_completion_queue(), deadline_, tag);
    }
};

template <class Deadline>
AlarmInitFunction(grpc::Alarm&, const Deadline&) -> AlarmInitFunction<Deadline>;

struct AlarmCancellationFunction
{
    grpc::Alarm& alarm_;

    explicit AlarmCancellationFunction(grpc::Alarm& alarm) noexcept : alarm_(alarm) {}

    template <class Deadline>
    explicit AlarmCancellationFunction(const detail::AlarmInitFunction<Deadline>& init_function) noexcept
        : alarm_(init_function.alarm_)
    {
    }

    void operator()() const { alarm_.Cancel(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type) const
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            operator()();
        }
    }
#endif
};

template <class Executor>
struct MoveAlarmSenderImplementation
{
    static constexpr bool NEEDS_ON_COMPLETE = true;

    using BaseType = detail::GrpcTagOperationBase;
    using Alarm = agrpc::BasicAlarm<Executor>;
    using Signature = void(bool, Alarm);
    using StopFunction = detail::AlarmCancellationFunction;

    template <class OnComplete>
    void complete(OnComplete on_complete, bool ok)
    {
        on_complete(ok, static_cast<Alarm&&>(alarm_));
    }

    auto& grpc_alarm() { return alarm_.alarm_; }

    Alarm alarm_;
};

template <class Executor>
struct SenderMoveAlarmSenderImplementation : MoveAlarmSenderImplementation<Executor>
{
    using Alarm = agrpc::BasicAlarm<Executor>;
    using Signature = void(Alarm);

    template <class OnComplete>
    void complete(OnComplete on_complete, bool ok)
    {
        if (ok)
        {
            on_complete(static_cast<Alarm&&>(this->alarm_));
        }
        else
        {
            on_complete.done();
        }
    }
};

template <class Deadline>
struct MoveAlarmSenderInitiation
{
    template <class Executor>
    static auto& stop_function_arg(MoveAlarmSenderImplementation<Executor>& impl) noexcept
    {
        return impl.grpc_alarm();
    }

    template <class Executor>
    void initiate(agrpc::GrpcContext& grpc_context, MoveAlarmSenderImplementation<Executor>& impl, void* tag) const
    {
        detail::AlarmInitFunction{impl.grpc_alarm(), deadline_}(grpc_context, tag);
    }

    Deadline deadline_;
};

struct SenderAlarmSenderImplementation : detail::GrpcSenderImplementation<detail::AlarmCancellationFunction>
{
    static constexpr bool NEEDS_ON_COMPLETE = true;

    using Signature = void();

    template <class OnComplete>
    static void complete(OnComplete on_complete, bool ok)
    {
        if (ok)
        {
            on_complete();
        }
        else
        {
            on_complete.done();
        }
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALARM_HPP
