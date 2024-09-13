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

#ifndef AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_DEFINITION_HPP
#define AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_DEFINITION_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/grpc_completion_queue_event.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/grpc_context_local_allocator.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/grpc_context.hpp>
#include <grpc/support/time.h>
#include <grpcpp/completion_queue.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
inline thread_local detail::GrpcContextThreadContext* thread_local_grpc_context{};

template <bool IsMultithreaded>
inline GrpcContextThreadContext::GrpcContextThreadContext(agrpc::GrpcContext& grpc_context,
                                                          std::bool_constant<IsMultithreaded>)
    : check_remote_work_{IsMultithreaded ? false : grpc_context.local_check_remote_work_},
      grpc_context_(grpc_context),
      local_work_queue_{IsMultithreaded ? decltype(local_work_queue_){} : std::move(grpc_context.local_work_queue_)},
      old_context_{std::exchange(detail::thread_local_grpc_context, this)},
      resource_{(old_context_ && &old_context_->grpc_context_ == &grpc_context)
                    ? old_context_->resource_
                    : GrpcContextImplementation::pop_resource(grpc_context)}
{
}

template <bool IsMultithreaded>
inline GrpcContextThreadContextImpl<IsMultithreaded>::GrpcContextThreadContextImpl(agrpc::GrpcContext& grpc_context)
    : GrpcContextThreadContext(grpc_context, std::bool_constant<IsMultithreaded>{})
{
}

template <bool IsMultithreaded>
inline GrpcContextThreadContextImpl<IsMultithreaded>::~GrpcContextThreadContextImpl() noexcept
{
    if constexpr (IsMultithreaded)
    {
        const bool moved_work = GrpcContextImplementation::move_local_queue_to_remote_work(*this);
        if (moved_work || check_remote_work_ ||
            (grpc_context_.is_stopped() && grpc_context_.remote_work_queue_.try_mark_active()))
        {
            GrpcContextImplementation::trigger_work_alarm(grpc_context_);
        }
    }
    else
    {
        grpc_context_.local_work_queue_ = std::move(local_work_queue_);
        grpc_context_.local_check_remote_work_ = check_remote_work_;
    }
    GrpcContextImplementation::push_resource(grpc_context_, resource_);
    detail::thread_local_grpc_context = old_context_;
}

inline void WorkFinishedOnExitFunctor::operator()() const noexcept { grpc_context_.work_finished(); }

inline bool GrpcContextImplementation::is_shutdown(const agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.shutdown_.load(std::memory_order_relaxed);
}

inline void GrpcContextImplementation::trigger_work_alarm(agrpc::GrpcContext& grpc_context) noexcept
{
    grpc_context.work_alarm_.Set(grpc_context.completion_queue_.get(), GrpcContextImplementation::TIME_ZERO,
                                 GrpcContextImplementation::CHECK_REMOTE_WORK_TAG);
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

inline bool GrpcContextImplementation::running_in_this_thread() noexcept { return detail::thread_local_grpc_context; }

inline bool GrpcContextImplementation::running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept
{
    const auto* context = detail::thread_local_grpc_context;
    return context && &grpc_context == &context->grpc_context_;
}

inline bool GrpcContextImplementation::move_local_queue_to_remote_work(
    detail::GrpcContextThreadContext& context) noexcept
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    return grpc_context.remote_work_queue_.prepend(std::move(context.local_work_queue_));
}

inline bool GrpcContextImplementation::move_remote_work_to_local_queue(
    detail::GrpcContextThreadContext& context) noexcept
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    return !grpc_context.remote_work_queue_.dequeue_all_and_try_mark_inactive(context.local_work_queue_);
}

inline bool GrpcContextImplementation::distribute_all_local_work_to_other_threads_but_one(
    detail::GrpcContextThreadContext& context) noexcept
{
    if (auto& local_work_queue = context.local_work_queue_; !local_work_queue.empty())
    {
        const auto first = local_work_queue.pop_front();
        const bool needs_trigger = GrpcContextImplementation::move_local_queue_to_remote_work(context);
        local_work_queue.push_back(first);
        return needs_trigger;
    }
    return false;
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

inline CompletionQueueEventResult GrpcContextImplementation::handle_next_completion_queue_event(
    detail::GrpcContextThreadContext& context, ::gpr_timespec deadline, detail::InvokeHandler invoke)
{
    agrpc::GrpcContext& grpc_context = context.grpc_context_;
    if (detail::GrpcCompletionQueueEvent event;
        detail::get_next_event(grpc_context.get_completion_queue(), event, deadline))
    {
        if (GrpcContextImplementation::CHECK_REMOTE_WORK_TAG == event.tag_)
        {
            context.check_remote_work_ = true;
            return {CompletionQueueEventResult::CHECK_REMOTE_WORK | CompletionQueueEventResult::HANDLED_EVENT};
        }
        const auto result =
            detail::InvokeHandler::NO_ == invoke
                ? (event.ok_ ? detail::OperationResult::SHUTDOWN_OK : detail::OperationResult::SHUTDOWN_NOT_OK)
                : (event.ok_ ? detail::OperationResult::OK_ : detail::OperationResult::NOT_OK);
        detail::process_grpc_tag(event.tag_, result, grpc_context);
        return {CompletionQueueEventResult::HANDLED_EVENT};
    }
    return {};
}

template <bool IsMultithreaded>
inline DoOneResult GrpcContextImplementation::do_one(detail::GrpcContextThreadContextImpl<IsMultithreaded>& context,
                                                     ::gpr_timespec deadline, detail::InvokeHandler invoke)
{
    const agrpc::GrpcContext& grpc_context = context.grpc_context_;
    const auto& local_work_queue = context.local_work_queue_;
    bool check_remote_work = context.check_remote_work_;
    if constexpr (IsMultithreaded)
    {
        if (local_work_queue.empty() && check_remote_work)
        {
            check_remote_work = GrpcContextImplementation::move_remote_work_to_local_queue(context);
        }
        if (GrpcContextImplementation::distribute_all_local_work_to_other_threads_but_one(context) || check_remote_work)
        {
            GrpcContextImplementation::trigger_work_alarm(context.grpc_context_);
        }
        check_remote_work = false;
    }
    else
    {
        if (check_remote_work)
        {
            check_remote_work = GrpcContextImplementation::move_remote_work_to_local_queue(context);
        }
    }
    context.check_remote_work_ = check_remote_work;
    const bool processed_local_work = GrpcContextImplementation::process_local_queue(context, invoke);
    if constexpr (IsMultithreaded)
    {
        if (GrpcContextImplementation::distribute_all_local_work_to_other_threads_but_one(context))
        {
            GrpcContextImplementation::trigger_work_alarm(context.grpc_context_);
        }
    }
    const bool is_more_completed_work_pending = check_remote_work || !local_work_queue.empty();
    if (!is_more_completed_work_pending && grpc_context.is_stopped())
    {
        return {{DoOneResult::PROCESSED_LOCAL_WORK}};
    }
    const auto handled_event = GrpcContextImplementation::handle_next_completion_queue_event(
        context, is_more_completed_work_pending ? GrpcContextImplementation::TIME_ZERO : deadline, invoke);
    return DoOneResult::from(handled_event, processed_local_work);
}

template <bool IsMultithreaded>
inline DoOneResult GrpcContextImplementation::do_one_if_not_stopped(
    detail::GrpcContextThreadContextImpl<IsMultithreaded>& context, ::gpr_timespec deadline)
{
    if (context.grpc_context_.is_stopped())
    {
        return {};
    }
    return {GrpcContextImplementation::do_one(context, deadline, detail::InvokeHandler::YES_)};
}

inline DoOneResult GrpcContextImplementation::do_one_completion_queue(detail::GrpcContextThreadContext& context,
                                                                      ::gpr_timespec deadline)
{
    return {
        GrpcContextImplementation::handle_next_completion_queue_event(context, deadline, detail::InvokeHandler::YES_)};
}

inline DoOneResult GrpcContextImplementation::do_one_completion_queue_if_not_stopped(
    detail::GrpcContextThreadContext& context, ::gpr_timespec deadline)
{
    if (context.grpc_context_.is_stopped())
    {
        return {};
    }
    return {
        GrpcContextImplementation::handle_next_completion_queue_event(context, deadline, detail::InvokeHandler::YES_)};
}

template <class LoopFunction>
inline bool GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context, LoopFunction loop_function)
{
    const auto run = [&loop_function](auto& thread_context)
    {
        bool processed{};
        DoOneResult result;
        while ((result = loop_function(thread_context)))
        {
            processed = processed || loop_function.has_processed(result);
        }
        return processed;
    };
    if (GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        return GrpcContextImplementation::visit_is_multithreaded(
            grpc_context,
            [&](auto v)
            {
                return run(
                    static_cast<GrpcContextThreadContextImpl<decltype(v)::value>&>(*detail::thread_local_grpc_context));
            });
    }
    if (grpc_context.outstanding_work_.load(std::memory_order_relaxed) == 0)
    {
        grpc_context.stopped_.store(true, std::memory_order_relaxed);
        return false;
    }
    grpc_context.reset();
    return GrpcContextImplementation::visit_is_multithreaded(
        grpc_context,
        [&](auto v)
        {
            detail::GrpcContextThreadContextImpl<decltype(v)::value> thread_context{grpc_context};
            return run(thread_context);
        });
}

inline void GrpcContextImplementation::drain_completion_queue(agrpc::GrpcContext& grpc_context) noexcept
{
    detail::GrpcContextThreadContextImpl<false> thread_context{grpc_context};
    (void)grpc_context.remote_work_queue_.try_mark_active();
    (void)GrpcContextImplementation::move_remote_work_to_local_queue(thread_context);
    GrpcContextImplementation::process_local_queue(thread_context, detail::InvokeHandler::NO_);
    while (GrpcContextImplementation::handle_next_completion_queue_event(
               thread_context, detail::GrpcContextImplementation::INFINITE_FUTURE, detail::InvokeHandler::NO_)
               .handled_event())
    {
        //
    }
}

inline detail::ListablePoolResource& GrpcContextImplementation::pop_resource(agrpc::GrpcContext& grpc_context)
{
    std::lock_guard guard{grpc_context.memory_resources_mutex_};
    auto& resources = grpc_context.memory_resources_;
    if (resources.empty())
    {
        return *(new detail::ListablePoolResource{});
    }
    return resources.pop_front();
}

inline void GrpcContextImplementation::push_resource(agrpc::GrpcContext& grpc_context,
                                                     detail::ListablePoolResource& resource)
{
    std::lock_guard guard{grpc_context.memory_resources_mutex_};
    grpc_context.memory_resources_.push_front(resource);
}

template <class Function>
inline decltype(auto) GrpcContextImplementation::visit_is_multithreaded(const agrpc::GrpcContext& grpc_context,
                                                                        Function function)
{
    if (grpc_context.multithreaded_)
    {
        return function(std::true_type{});
    }
    return function(std::false_type{});
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

inline detail::GrpcContextLocalAllocator get_local_allocator() noexcept { return detail::GrpcContextLocalAllocator(); }

inline detail::PoolResource& get_local_pool_resource() noexcept
{
    return detail::thread_local_grpc_context->resource_.resource_;
}

template <class T>
inline T* PoolResourceAllocator<T>::allocate(std::size_t n)
{
    if constexpr (alignof(T) > MAX_ALIGN)
    {
        return std::allocator<T>{}.allocate(n);
    }
    else
    {
        const auto allocation_size = n * sizeof(T);
        if (allocation_size > LARGEST_POOL_BLOCK_SIZE)
        {
            return std::allocator<T>{}.allocate(n);
        }
        return static_cast<T*>(detail::get_local_pool_resource().allocate(allocation_size));
    }
}

template <class T>
inline void PoolResourceAllocator<T>::deallocate(T* p, std::size_t n) noexcept
{
    if constexpr (alignof(T) > MAX_ALIGN)
    {
        return std::allocator<T>{}.deallocate(p);
    }
    else
    {
        const auto allocation_size = n * sizeof(T);
        if (allocation_size > LARGEST_POOL_BLOCK_SIZE)
        {
            std::allocator<T>{}.deallocate(p, n);
        }
        else
        {
            detail::get_local_pool_resource().deallocate(p, allocation_size);
        }
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_DEFINITION_HPP
