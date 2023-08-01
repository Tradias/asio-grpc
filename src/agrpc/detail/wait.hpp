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

#ifndef AGRPC_DETAIL_WAIT_HPP
#define AGRPC_DETAIL_WAIT_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/grpc_context.hpp>
#include <grpcpp/alarm.h>

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

#if !defined(AGRPC_UNIFEX)
    explicit
#endif
        AlarmCancellationFunction(grpc::Alarm& alarm) noexcept
        : alarm_(alarm)
    {
    }

    template <class Deadline>
#if !defined(AGRPC_UNIFEX)
    explicit
#endif
        AlarmCancellationFunction(const detail::AlarmInitFunction<Deadline>& init_function) noexcept
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
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_WAIT_HPP
