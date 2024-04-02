// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_IPP
#define AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_IPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/grpc_completion_queue_event.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/grpc_context_local_allocator.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/grpc_context.hpp>
#include <grpc/support/time.h>
#include <grpcpp/completion_queue.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
inline thread_local detail::GrpcContextThreadContext* thread_local_grpc_context{};

inline GrpcLocalContext::GrpcLocalContext(agrpc::GrpcContext& grpc_context)
    : grpc_context_(grpc_context), check_remote_work_{false}
{
}

inline ThreadLocalGrpcContextGuard::ThreadLocalGrpcContextGuard(detail::GrpcContextThreadContext& context) noexcept
    : old_context_{std::exchange(detail::thread_local_grpc_context, &context)}
{
}

inline ThreadLocalGrpcContextGuard::~ThreadLocalGrpcContextGuard()
{
    auto* const context = detail::thread_local_grpc_context;
    bool check_remote_work = context->check_remote_work_;
    while (check_remote_work)
    {
        check_remote_work = GrpcContextImplementation::move_remote_work_to_local_queue(*context);
    }
    if (GrpcContextImplementation::move_local_queue_to_remote_work(*context))
    {
        GrpcContextImplementation::trigger_work_alarm(context->grpc_context_);
    }
    detail::thread_local_grpc_context = old_context_;
}

inline void WorkFinishedOnExitFunctor::operator()() const noexcept { grpc_context_.work_finished(); }

inline StartWorkAndGuard::StartWorkAndGuard(agrpc::GrpcContext& grpc_context) noexcept
    : detail::WorkFinishedOnExit(grpc_context)
{
    grpc_context.work_started();
}

inline bool IsGrpcContextStoppedPredicate::operator()(const agrpc::GrpcContext& grpc_context) const noexcept
{
    return grpc_context.is_stopped();
}

inline bool GrpcContextImplementation::is_shutdown(const agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.shutdown_.load(std::memory_order_relaxed);
}

inline void GrpcContextImplementation::trigger_work_alarm(agrpc::GrpcContext& grpc_context) noexcept
{
    grpc_context.work_alarm_.Set(grpc_context.completion_queue_.get(), GrpcContextImplementation::TIME_ZERO,
                                 GrpcContextImplementation::HAS_REMOTE_WORK_TAG);
}

inline void GrpcContextImplementation::work_started(agrpc::GrpcContext& grpc_context) noexcept
{
    grpc_context.work_started();
}

inline void GrpcContextImplementation::add_remote_operation(agrpc::GrpcContext& grpc_context,
                                                            detail::QueueableOperationBase* op) noexcept
{
    if (grpc_context.remote_work_queue_.enqueue(op))
    {
        GrpcContextImplementation::trigger_work_alarm(grpc_context);
    }
}

inline void GrpcContextImplementation::add_local_operation(detail::QueueableOperationBase* op) noexcept
{
    detail::thread_local_grpc_context->local_work_queue_.push_back(op);
}

inline void GrpcContextImplementation::add_operation(agrpc::GrpcContext& grpc_context,
                                                     detail::QueueableOperationBase* op) noexcept
{
    if (GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        GrpcContextImplementation::add_local_operation(op);
    }
    else
    {
        GrpcContextImplementation::add_remote_operation(grpc_context, op);
    }
}

inline bool GrpcContextImplementation::running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept
{
    return detail::thread_local_grpc_context && &grpc_context == &detail::thread_local_grpc_context->grpc_context_;
}

inline bool GrpcContextImplementation::move_local_queue_to_remote_work(
    detail::GrpcContextThreadContext& context) noexcept
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    bool is_queue_inactive = false;
    auto queue{std::move(context.local_work_queue_)};
    while (!queue.empty())
    {
        auto* op = queue.pop_front();
        if (grpc_context.remote_work_queue_.enqueue(op))
        {
            is_queue_inactive = true;
        }
    }
    return is_queue_inactive;
}

inline bool GrpcContextImplementation::move_remote_work_to_local_queue(
    detail::GrpcContextThreadContext& context) noexcept
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    auto remote_work_queue = grpc_context.remote_work_queue_.try_mark_inactive_or_dequeue_all();
    if (remote_work_queue.empty())
    {
        return false;
    }
    context.local_work_queue_.append(std::move(remote_work_queue));
    return true;
}

inline bool GrpcContextImplementation::process_local_queue(detail::GrpcContextThreadContext& context,
                                                           detail::InvokeHandler invoke)
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    bool processed{};
    const auto result =
        detail::InvokeHandler::NO_ == invoke ? detail::OperationResult::SHUTDOWN_NOT_OK : detail::OperationResult::OK_;
    auto queue{std::move(context.local_work_queue_)};
    while (!queue.empty())
    {
        processed = true;
        detail::WorkFinishedOnExit on_exit{grpc_context};
        auto* operation = queue.pop_front();
        operation->complete(result, grpc_context);
    }
    return processed;
}

inline bool get_next_event(grpc::CompletionQueue* cq, detail::GrpcCompletionQueueEvent& event,
                           ::gpr_timespec deadline) noexcept
{
    return grpc::CompletionQueue::GOT_EVENT == cq->AsyncNext(&event.tag_, &event.ok_, deadline);
}

inline bool GrpcContextImplementation::handle_next_completion_queue_event(detail::GrpcContextThreadContext& context,
                                                                          ::gpr_timespec deadline,
                                                                          detail::InvokeHandler invoke)
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    if (detail::GrpcCompletionQueueEvent event;
        detail::get_next_event(grpc_context.get_completion_queue(), event, deadline))
    {
        if (GrpcContextImplementation::HAS_REMOTE_WORK_TAG == event.tag_)
        {
            context.check_remote_work_ = true;
        }
        else
        {
            const auto result =
                detail::InvokeHandler::NO_ == invoke
                    ? (event.ok_ ? detail::OperationResult::SHUTDOWN_OK : detail::OperationResult::SHUTDOWN_NOT_OK)
                    : (event.ok_ ? detail::OperationResult::OK_ : detail::OperationResult::NOT_OK);
            detail::process_grpc_tag(event.tag_, result, grpc_context);
        }
        return true;
    }
    return false;
}

template <class StopPredicate>
inline bool GrpcContextImplementation::do_one(detail::GrpcContextThreadContext& context, ::gpr_timespec deadline,
                                              detail::InvokeHandler invoke, StopPredicate stop_predicate)
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    bool processed{};
    bool check_remote_work = context.check_remote_work_;
    while (check_remote_work)
    {
        check_remote_work = GrpcContextImplementation::move_remote_work_to_local_queue(context);
    }
    context.check_remote_work_ = false;
    const bool processed_local_operation = GrpcContextImplementation::process_local_queue(context, invoke);
    processed = processed || processed_local_operation;
    const bool is_more_completed_work_pending = check_remote_work || !context.local_work_queue_.empty();
    if (!is_more_completed_work_pending && stop_predicate(grpc_context))
    {
        return processed;
    }
    const bool handled_event = GrpcContextImplementation::handle_next_completion_queue_event(
        context, is_more_completed_work_pending ? GrpcContextImplementation::TIME_ZERO : deadline, invoke);
    return processed || handled_event;
}

inline bool GrpcContextImplementation::do_one_if_not_stopped(detail::GrpcContextThreadContext& context,
                                                             ::gpr_timespec deadline)
{
    if (context.grpc_context_.is_stopped())
    {
        return false;
    }
    return GrpcContextImplementation::do_one(context, deadline, detail::InvokeHandler::YES_);
}

inline bool GrpcContextImplementation::do_one_completion_queue(detail::GrpcContextThreadContext& context,
                                                               ::gpr_timespec deadline)
{
    return GrpcContextImplementation::handle_next_completion_queue_event(context, deadline,
                                                                         detail::InvokeHandler::YES_);
}

inline bool GrpcContextImplementation::do_one_completion_queue_if_not_stopped(detail::GrpcContextThreadContext& context,
                                                                              ::gpr_timespec deadline)
{
    if (context.grpc_context_.is_stopped())
    {
        return false;
    }
    return GrpcContextImplementation::handle_next_completion_queue_event(context, deadline,
                                                                         detail::InvokeHandler::YES_);
}

template <class LoopFunction>
inline bool GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, LoopFunction loop_function)
{
    if (GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        bool processed{};
        while (loop_function(*detail::thread_local_grpc_context))
        {
            processed = true;
        }
        return processed;
    }
    if (grpc_context.outstanding_work_.load(std::memory_order_relaxed) == 0)
    {
        grpc_context.stopped_.store(true, std::memory_order_relaxed);
        return false;
    }
    grpc_context.reset();
    detail::GrpcContextThreadContext thread_context{grpc_context};
    detail::ThreadLocalGrpcContextGuard guard{thread_context};
    bool processed{};
    while (loop_function(thread_context))
    {
        processed = true;
    }
    return processed;
}

inline void process_grpc_tag(void* tag, detail::OperationResult result, agrpc::GrpcContext& grpc_context)
{
    detail::WorkFinishedOnExit on_exit{grpc_context};
    auto* operation = static_cast<detail::OperationBase*>(tag);
    operation->complete(result, grpc_context);
}

inline ::gpr_timespec gpr_timespec_from_now(std::chrono::nanoseconds duration) noexcept
{
    const auto duration_timespec = ::gpr_time_from_nanos(duration.count(), GPR_TIMESPAN);
    const auto timespec = ::gpr_now(GPR_CLOCK_MONOTONIC);
    return ::gpr_time_add(timespec, duration_timespec);
}

inline detail::GrpcContextLocalAllocator get_local_allocator(agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.get_allocator();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_IPP
