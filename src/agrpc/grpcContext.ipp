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

#ifndef AGRPC_AGRPC_GRPCCONTEXT_IPP
#define AGRPC_AGRPC_GRPCCONTEXT_IPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/attributes.hpp"
#include "agrpc/detail/grpcCompletionQueueEvent.hpp"
#include "agrpc/detail/grpcExecutorOptions.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/grpcContext.hpp"
#include "agrpc/grpcExecutor.hpp"

#include <boost/asio/execution_context.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/lockfree/queue.hpp>
#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#include <atomic>
#include <memory_resource>
#include <thread>
#include <utility>

namespace agrpc
{
namespace detail
{
struct GrpcContextThreadInfo : asio::detail::thread_info_base
{
};

// Enables Boost.Asio's awaitable frame memory recycling
struct GrpcContextThreadContext : asio::detail::thread_context
{
    GrpcContextThreadInfo this_thread;
    thread_call_stack::context ctx{this, this_thread};
};

inline bool get_next_event(agrpc::GrpcContext& grpc_context, detail::GrpcCompletionQueueEvent& event)
{
    static constexpr ::gpr_timespec INFINITE_FUTURE{std::numeric_limits<std::int64_t>::max(), 0, ::GPR_CLOCK_MONOTONIC};
    return grpc_context.get_completion_queue()->AsyncNext(&event.tag, &event.ok, INFINITE_FUTURE) !=
           grpc::CompletionQueue::SHUTDOWN;
}

template <class LoopPredicate>
void run_event_loop(agrpc::GrpcContext& grpc_context, LoopPredicate loop_predicate)
{
    detail::GrpcCompletionQueueEvent event;
    while (loop_predicate() && detail::get_next_event(grpc_context, event))
    {
        if (grpc_context.is_stopped()) AGRPC_UNLIKELY
            {
                detail::GrpcContextImplementation::process_work<detail::InvokeHandler::NO>(grpc_context, event);
            }
        else
        {
            detail::GrpcContextImplementation::process_work<detail::InvokeHandler::YES>(grpc_context, event);
        }
    }
}
}  // namespace detail

inline GrpcContext::GrpcContext(std::unique_ptr<grpc::CompletionQueue> completion_queue,
                                std::pmr::memory_resource* local_upstream_resource)
    : work_alarm(),
      outstanding_work(),
      thread_id(std::this_thread::get_id()),
      stopped(),
      has_work(),
      completion_queue(std::move(completion_queue)),
      local_resource(local_upstream_resource),
      local_work_queue(),
      is_processing_local_work(false),
      remote_work_queue(32)
{
}

inline GrpcContext::~GrpcContext()
{
    this->stop();
    this->completion_queue->Shutdown();
    detail::run_event_loop(*this, detail::Always{true});
    asio::execution_context::shutdown();
    asio::execution_context::destroy();
}

inline void GrpcContext::run()
{
    if (this->outstanding_work.load(std::memory_order_relaxed) == 0)
    {
        return;
    }
    this->reset();
    detail::GrpcContextThreadContext thread_context;
    this->thread_id.store(std::this_thread::get_id(), std::memory_order_relaxed);
    detail::run_event_loop(*this,
                           [&]()
                           {
                               return !this->is_stopped();
                           });
}

inline void GrpcContext::stop()
{
    if (!this->stopped.exchange(true, std::memory_order_relaxed))
    {
        detail::GrpcContextImplementation::trigger_work_alarm(*this);
    }
}

inline void GrpcContext::reset() noexcept { this->stopped.store(false, std::memory_order_relaxed); }

inline bool GrpcContext::is_stopped() const noexcept { return this->stopped.load(std::memory_order_relaxed); }

inline GrpcContext::executor_type GrpcContext::get_executor() noexcept { return GrpcContext::executor_type{*this}; }

inline GrpcContext::allocator_type GrpcContext::get_allocator() noexcept
{
    return GrpcContext::allocator_type{&this->local_resource};
}

inline void GrpcContext::work_started() noexcept { this->outstanding_work.fetch_add(1, std::memory_order_relaxed); }

inline void GrpcContext::work_finished() noexcept
{
    if (this->outstanding_work.fetch_sub(1, std::memory_order_relaxed) == 1) AGRPC_UNLIKELY
        {
            this->stop();
        }
}

inline grpc::CompletionQueue* GrpcContext::get_completion_queue() noexcept { return this->completion_queue.get(); }

inline grpc::ServerCompletionQueue* GrpcContext::get_server_completion_queue() noexcept
{
    return static_cast<grpc::ServerCompletionQueue*>(this->completion_queue.get());
}
}  // namespace agrpc

#endif  // AGRPC_AGRPC_GRPCCONTEXT_IPP
