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
#include <grpcpp/completion_queue.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct GrpcContextThreadInfo : asio::detail::thread_info_base
{
};

// Enables Boost.Asio's awaitable frame memory recycling
struct GrpcContextThreadContext : asio::detail::thread_context
{
    GrpcContextThreadInfo this_thread;
    thread_call_stack::context ctx{this, this_thread};
};
#endif

inline thread_local const agrpc::GrpcContext* thread_local_grpc_context{};

struct ThreadLocalGrpcContextGuard
{
    const agrpc::GrpcContext* old_context;

    explicit ThreadLocalGrpcContextGuard(const agrpc::GrpcContext& grpc_context) noexcept
        : old_context{detail::GrpcContextImplementation::set_thread_local_grpc_context(&grpc_context)}
    {
    }

    ~ThreadLocalGrpcContextGuard() { detail::GrpcContextImplementation::set_thread_local_grpc_context(old_context); }

    ThreadLocalGrpcContextGuard(const ThreadLocalGrpcContextGuard&) = delete;
    ThreadLocalGrpcContextGuard(ThreadLocalGrpcContextGuard&&) = delete;
    ThreadLocalGrpcContextGuard& operator=(const ThreadLocalGrpcContextGuard&) = delete;
    ThreadLocalGrpcContextGuard& operator=(ThreadLocalGrpcContextGuard&&) = delete;
};

struct IsGrpcContextStoppedCondition
{
    const agrpc::GrpcContext& grpc_context;

    bool operator()() const noexcept { return grpc_context.is_stopped(); }
};

inline void WorkFinishedOnExitFunctor::operator()() const noexcept { grpc_context.work_finished(); }

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

template <detail::InvokeHandler Invoke>
void GrpcContextImplementation::process_local_queue(agrpc::GrpcContext& grpc_context)
{
    auto queue{std::move(grpc_context.local_work_queue)};
    while (!queue.empty())
    {
        detail::WorkFinishedOnExit on_exit{grpc_context};
        auto* operation = queue.pop_front();
        operation->complete(Invoke, grpc_context.get_allocator());
    }
}

inline bool get_next_event(grpc::CompletionQueue* cq, detail::GrpcCompletionQueueEvent& event,
                           ::gpr_timespec deadline) noexcept
{
    return grpc::CompletionQueue::GOT_EVENT == cq->AsyncNext(&event.tag, &event.ok, deadline);
}

template <detail::InvokeHandler Invoke, class StopCondition>
bool GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, StopCondition stop_condition,
                                             ::gpr_timespec deadline)
{
    if (grpc_context.check_remote_work)
    {
        grpc_context.check_remote_work =
            detail::GrpcContextImplementation::move_remote_work_to_local_queue(grpc_context);
    }
    detail::GrpcContextImplementation::process_local_queue<Invoke>(grpc_context);
    if (stop_condition())
    {
        return false;
    }
    const auto is_more_completed_work_pending =
        grpc_context.check_remote_work || !grpc_context.local_work_queue.empty();
    if (detail::GrpcCompletionQueueEvent event; detail::get_next_event(
            grpc_context.get_completion_queue(), event,
            is_more_completed_work_pending ? detail::GrpcContextImplementation::TIME_ZERO : deadline))
    {
        if (detail::GrpcContextImplementation::HAS_REMOTE_WORK_TAG == event.tag)
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
    return is_more_completed_work_pending;
}

inline bool GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline)
{
    if (grpc_context.outstanding_work.load(std::memory_order_relaxed) == 0)
    {
        grpc_context.stopped.store(true, std::memory_order_relaxed);
        return false;
    }
    grpc_context.reset();
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    detail::GrpcContextThreadContext thread_context;
#endif
    detail::ThreadLocalGrpcContextGuard guard{grpc_context};
    bool processed{};
    while (detail::GrpcContextImplementation::process_work<detail::InvokeHandler::YES>(
        grpc_context, detail::IsGrpcContextStoppedCondition{grpc_context}, deadline))

    {
        processed = true;
    }
    return processed;
}

inline bool GrpcContextImplementation::run(agrpc::GrpcContext& grpc_context)
{
    return detail::GrpcContextImplementation::process_work(grpc_context,
                                                           detail::GrpcContextImplementation::INFINITE_FUTURE);
}

inline bool GrpcContextImplementation::poll(agrpc::GrpcContext& grpc_context)
{
    return detail::GrpcContextImplementation::process_work(grpc_context, detail::GrpcContextImplementation::TIME_ZERO);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_IPP
