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
#include "agrpc/detail/typeErasedOperation.hpp"
#include "agrpc/grpcContext.hpp"

#include <cstdint>
#include <limits>

namespace agrpc::detail
{
inline thread_local const agrpc::GrpcContext* thread_local_grpc_context{};

inline void WorkFinishedOnExitFunctor::operator()() const noexcept { grpc_context.work_finished(); }

inline void GrpcContextImplementation::trigger_work_alarm(agrpc::GrpcContext& grpc_context)
{
    static constexpr ::gpr_timespec TIME_ZERO{std::numeric_limits<std::int64_t>::min(), 0, ::GPR_CLOCK_MONOTONIC};
    grpc_context.work_alarm.Set(grpc_context.completion_queue.get(), TIME_ZERO,
                                detail::GrpcContextImplementation::HAS_REMOTE_WORK_TAG);
}

inline void GrpcContextImplementation::add_remote_operation(agrpc::GrpcContext& grpc_context,
                                                            detail::TypeErasedNoArgOperation* op)
{
    grpc_context.work_started();
    if (grpc_context.remote_work_queue.enqueue(op))
    {
        detail::GrpcContextImplementation::trigger_work_alarm(grpc_context);
    }
}

inline void GrpcContextImplementation::add_local_operation(agrpc::GrpcContext& grpc_context,
                                                           detail::TypeErasedNoArgOperation* op)
{
    grpc_context.work_started();
    grpc_context.local_work_queue.push_back(op);
}

inline bool GrpcContextImplementation::get_next_event(agrpc::GrpcContext& grpc_context,
                                                      detail::GrpcCompletionQueueEvent& event)
{
    static constexpr ::gpr_timespec INFINITE_FUTURE{std::numeric_limits<std::int64_t>::max(), 0, ::GPR_CLOCK_MONOTONIC};
    return grpc_context.get_completion_queue()->AsyncNext(&event.tag, &event.ok, INFINITE_FUTURE) !=
           grpc::CompletionQueue::SHUTDOWN;
}

inline bool GrpcContextImplementation::running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept
{
    return std::addressof(grpc_context) == detail::thread_local_grpc_context;
}

inline const agrpc::GrpcContext* GrpcContextImplementation::set_thread_local_grpc_context(
    const agrpc::GrpcContext* grpc_context) noexcept
{
    return std::exchange(detail::thread_local_grpc_context, grpc_context);
}

inline void GrpcContextImplementation::move_remote_work_to_local_queue(agrpc::GrpcContext& grpc_context) noexcept
{
    while (true)
    {
        auto remote_work_queue = grpc_context.remote_work_queue.try_mark_inactive_or_dequeue_all();
        if (remote_work_queue.empty())
        {
            break;
        }
        grpc_context.local_work_queue.append(std::move(remote_work_queue));
    }
}

template <detail::InvokeHandler Invoke>
void GrpcContextImplementation::process_local_queue(agrpc::GrpcContext& grpc_context)
{
    while (!grpc_context.local_work_queue.empty())
    {
        detail::WorkFinishedOnExit on_exit{grpc_context};
        auto* operation = grpc_context.local_work_queue.pop_front();
        operation->complete(Invoke, grpc_context.get_allocator());
    }
}

template <detail::InvokeHandler Invoke, class IsStoppedPredicate>
bool GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, IsStoppedPredicate is_stopped_predicate)
{
    if (grpc_context.check_remote_work)
    {
        detail::GrpcContextImplementation::move_remote_work_to_local_queue(grpc_context);
        grpc_context.check_remote_work = false;
    }
    detail::GrpcContextImplementation::process_local_queue<Invoke>(grpc_context);
    if (is_stopped_predicate())
    {
        return false;
    }
    if (detail::GrpcCompletionQueueEvent event; detail::GrpcContextImplementation::get_next_event(grpc_context, event))
    {
        if (event.tag == detail::GrpcContextImplementation::HAS_REMOTE_WORK_TAG)
        {
            grpc_context.check_remote_work = true;
        }
        else
        {
            detail::WorkFinishedOnExit on_exit{grpc_context};
            auto* operation = static_cast<detail::TypeErasedGrpcTagOperation*>(event.tag);
            operation->complete(Invoke, event.ok, grpc_context.get_allocator());
        }
        return true;
    }
    return false;
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP
