// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP
#define AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP

#include "agrpc/detail/grpcCompletionQueueEvent.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/operation.hpp"
#include "agrpc/detail/typeErasedOperation.hpp"
#include "agrpc/grpcContext.hpp"

#include <cstdint>
#include <limits>

namespace agrpc::detail
{
inline void GrpcContextImplementation::trigger_work_alarm(agrpc::GrpcContext& grpc_context)
{
    static constexpr ::gpr_timespec TIME_ZERO{std::numeric_limits<std::int64_t>::min(), 0, ::GPR_CLOCK_MONOTONIC};
    if (!grpc_context.has_work.exchange(true, std::memory_order_acquire))
    {
        grpc_context.work_alarm.Set(grpc_context.completion_queue.get(), TIME_ZERO,
                                    detail::GrpcContextImplementation::HAS_WORK_TAG);
    }
}

inline void GrpcContextImplementation::add_remote_operation(agrpc::GrpcContext& grpc_context,
                                                            detail::TypeErasedNoArgRemoteOperation* op)
{
    grpc_context.remote_work_queue.push(op);
    detail::GrpcContextImplementation::trigger_work_alarm(grpc_context);
}

inline void GrpcContextImplementation::add_local_operation(agrpc::GrpcContext& grpc_context,
                                                           detail::TypeErasedNoArgLocalOperation* op)
{
    grpc_context.local_work_queue.push_back(*op);
    if (!grpc_context.is_processing_local_work)
    {
        detail::GrpcContextImplementation::trigger_work_alarm(grpc_context);
    }
}

inline bool GrpcContextImplementation::running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.thread_id.load(std::memory_order_relaxed) == std::this_thread::get_id();
}

template <detail::InvokeHandler Invoke>
void GrpcContextImplementation::process_local_queue(agrpc::GrpcContext& grpc_context)
{
    while (!grpc_context.local_work_queue.empty())
    {
        grpc_context.is_processing_local_work = true;
        auto& operation = grpc_context.local_work_queue.front();
        grpc_context.local_work_queue.pop_front();
        operation.complete(Invoke, grpc_context.get_allocator());
    }
    grpc_context.is_processing_local_work = false;
}

template <detail::InvokeHandler Invoke>
void GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, detail::GrpcCompletionQueueEvent event)
{
    if (event.tag == detail::GrpcContextImplementation::HAS_WORK_TAG)
    {
        detail::GrpcContextImplementation::process_local_queue<Invoke>(grpc_context);
        grpc_context.has_work.store(false, std::memory_order_release);
        grpc_context.remote_work_queue.consume_all(
            [](detail::TypeErasedNoArgRemoteOperation* operation)
            {
                operation->complete(Invoke);
            });
    }
    else
    {
        auto* operation = static_cast<detail::TypeErasedGrpcTagOperation*>(event.tag);
        operation->complete(Invoke, event.ok, grpc_context.get_allocator());
    }
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP
