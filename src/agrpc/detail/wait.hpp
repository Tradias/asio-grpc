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

#ifndef AGRPC_DETAIL_WAIT_HPP
#define AGRPC_DETAIL_WAIT_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/grpcContext.hpp>
#include <grpcpp/alarm.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Deadline>
struct AlarmInitFunction
{
    grpc::Alarm& alarm;
    Deadline deadline;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        alarm.Set(grpc_context.get_completion_queue(), deadline, tag);
    }
};

template <class Deadline>
AlarmInitFunction(grpc::Alarm&, const Deadline&) -> AlarmInitFunction<Deadline>;

struct AlarmCancellationHandler
{
    grpc::Alarm& alarm;

    constexpr explicit AlarmCancellationHandler(grpc::Alarm& alarm) noexcept : alarm(alarm) {}

    template <class Deadline>
    constexpr explicit AlarmCancellationHandler(const detail::AlarmInitFunction<Deadline>& init_function) noexcept
        : alarm(init_function.alarm)
    {
    }

    void operator()() { alarm.Cancel(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type)
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            (*this)();
        }
    }
#endif
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_WAIT_HPP
