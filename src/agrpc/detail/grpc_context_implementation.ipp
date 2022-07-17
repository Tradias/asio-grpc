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

#ifndef AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP
#define AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/grpcCompletionQueueEvent.hpp>
#include <agrpc/detail/grpcContextImplementation.hpp>
#include <agrpc/detail/typeErasedOperation.hpp>
#include <agrpc/grpcContext.hpp>
#include <grpc/support/time.h>
#include <grpcpp/completion_queue.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
inline thread_local const agrpc::GrpcContext* thread_local_grpc_context{};

inline ThreadLocalGrpcContextGuard::ThreadLocalGrpcContextGuard(const agrpc::GrpcContext& grpc_context) noexcept
    : old_context{detail::GrpcContextImplementation::set_thread_local_grpc_context(&grpc_context)}
{
}

inline ThreadLocalGrpcContextGuard::~ThreadLocalGrpcContextGuard()
{
    detail::GrpcContextImplementation::set_thread_local_grpc_context(old_context);
}

inline void WorkFinishedOnExitFunctor::operator()() const noexcept { grpc_context.work_finished(); }

inline bool IsGrpcContextStoppedPredicate::operator()(const agrpc::GrpcContext& grpc_context) const noexcept
{
    return grpc_context.is_stopped();
}

inline bool GrpcContextImplementation::is_shutdown(const agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.shutdown.load(std::memory_order_relaxed);
}

inline void GrpcContextImplementation::trigger_work_alarm(agrpc::GrpcContext& grpc_context) noexcept
{
    grpc_context.work_alarm.Set(grpc_context.completion_queue.get(), detail::GrpcContextImplementation::TIME_ZERO,
                                detail::GrpcContextImplementation::HAS_REMOTE_WORK_TAG);
}

inline void GrpcContextImplementation::add_remote_operation(agrpc::GrpcContext& grpc_context,
                                                            detail::TypeErasedNoArgOperation* op) noexcept
{
    if (grpc_context.remote_work_queue.enqueue(op))
    {
        detail::GrpcContextImplementation::trigger_work_alarm(grpc_context);
    }
}

inline void GrpcContextImplementation::add_local_operation(agrpc::GrpcContext& grpc_context,
                                                           detail::TypeErasedNoArgOperation* op) noexcept
{
    grpc_context.local_work_queue.push_back(op);
}

inline void GrpcContextImplementation::add_operation(agrpc::GrpcContext& grpc_context,
                                                     detail::TypeErasedNoArgOperation* op) noexcept
{
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        detail::GrpcContextImplementation::add_local_operation(grpc_context, op);
    }
    else
    {
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, op);
    }
}

inline bool GrpcContextImplementation::running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept
{
    return &grpc_context == detail::thread_local_grpc_context;
}

inline const agrpc::GrpcContext* GrpcContextImplementation::set_thread_local_grpc_context(
    const agrpc::GrpcContext* grpc_context) noexcept
{
    return std::exchange(detail::thread_local_grpc_context, grpc_context);
}

inline bool GrpcContextImplementation::move_remote_work_to_local_queue(agrpc::GrpcContext& grpc_context) noexcept
{
    auto remote_work_queue = grpc_context.remote_work_queue.try_mark_inactive_or_dequeue_all();
    if (remote_work_queue.empty())
    {
        return false;
    }
    grpc_context.local_work_queue.append(std::move(remote_work_queue));
    return true;
}

inline bool GrpcContextImplementation::process_local_queue(agrpc::GrpcContext& grpc_context,
                                                           detail::InvokeHandler invoke)
{
    bool processed{};
    auto queue{std::move(grpc_context.local_work_queue)};
    while (!queue.empty())
    {
        processed = true;
        detail::WorkFinishedOnExit on_exit{grpc_context};
        auto* operation = queue.pop_front();
        operation->complete(invoke, grpc_context.get_allocator());
    }
    return processed;
}

inline bool get_next_event(grpc::CompletionQueue* cq, detail::GrpcCompletionQueueEvent& event,
                           ::gpr_timespec deadline) noexcept
{
    return grpc::CompletionQueue::GOT_EVENT == cq->AsyncNext(&event.tag, &event.ok, deadline);
}

inline bool GrpcContextImplementation::handle_next_completion_queue_event(agrpc::GrpcContext& grpc_context,
                                                                          ::gpr_timespec deadline,
                                                                          detail::InvokeHandler invoke)
{
    if (detail::GrpcCompletionQueueEvent event;
        detail::get_next_event(grpc_context.get_completion_queue(), event, deadline))
    {
        if (detail::GrpcContextImplementation::HAS_REMOTE_WORK_TAG == event.tag)
        {
            grpc_context.check_remote_work = true;
        }
        else
        {
            detail::process_grpc_tag(event.tag, invoke, event.ok, grpc_context);
        }
        return true;
    }
    return false;
}

template <class StopPredicate>
bool GrpcContextImplementation::do_one(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline,
                                       detail::InvokeHandler invoke, StopPredicate stop_predicate)
{
    if (stop_predicate(grpc_context))
    {
        return false;
    }
    bool processed{};
    bool check_remote_work = grpc_context.check_remote_work;
    if (check_remote_work)
    {
        check_remote_work = detail::GrpcContextImplementation::move_remote_work_to_local_queue(grpc_context);
        grpc_context.check_remote_work = check_remote_work;
    }
    const bool processed_local_operation = detail::GrpcContextImplementation::process_local_queue(grpc_context, invoke);
    processed = processed || processed_local_operation;
    const bool is_more_completed_work_pending = check_remote_work || !grpc_context.local_work_queue.empty();
    if (!is_more_completed_work_pending && stop_predicate(grpc_context))
    {
        return processed;
    }
    const bool handled_event = detail::GrpcContextImplementation::handle_next_completion_queue_event(
        grpc_context, is_more_completed_work_pending ? detail::GrpcContextImplementation::TIME_ZERO : deadline, invoke);
    return processed || handled_event;
}

inline bool GrpcContextImplementation::do_one_completion_queue(agrpc::GrpcContext& grpc_context,
                                                               ::gpr_timespec deadline, detail::InvokeHandler invoke)
{
    if (grpc_context.is_stopped())
    {
        return false;
    }
    return detail::GrpcContextImplementation::handle_next_completion_queue_event(grpc_context, deadline, invoke);
}

template <class LoopFunction>
inline bool GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, LoopFunction loop_function)
{
    if (grpc_context.outstanding_work.load(std::memory_order_relaxed) == 0)
    {
        grpc_context.stopped.store(true, std::memory_order_relaxed);
        return false;
    }
    grpc_context.reset();
    [[maybe_unused]] detail::GrpcContextThreadContext thread_context;
    detail::ThreadLocalGrpcContextGuard guard{grpc_context};
    bool processed{};
    while (loop_function(grpc_context))
    {
        processed = true;
    }
    return processed;
}

inline void process_grpc_tag(void* tag, detail::InvokeHandler invoke, bool ok, agrpc::GrpcContext& grpc_context)
{
    detail::WorkFinishedOnExit on_exit{grpc_context};
    auto* operation = static_cast<detail::TypeErasedGrpcTagOperation*>(tag);
    operation->complete(invoke, ok, grpc_context.get_allocator());
}

inline ::gpr_timespec gpr_timespec_from_now(std::chrono::nanoseconds duration) noexcept
{
    const auto duration_timespec = ::gpr_time_from_nanos(duration.count(), GPR_TIMESPAN);
    const auto timespec = ::gpr_now(GPR_CLOCK_MONOTONIC);
    return ::gpr_time_add(timespec, duration_timespec);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP
