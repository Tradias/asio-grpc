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

#ifndef AGRPC_DETAIL_GRPCCONTEXT_IPP
#define AGRPC_DETAIL_GRPCCONTEXT_IPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpcCompletionQueueEvent.hpp>
#include <agrpc/detail/grpcContext.hpp>
#include <agrpc/detail/grpcExecutorOptions.hpp>
#include <agrpc/detail/intrusiveQueue.hpp>
#include <agrpc/detail/memoryResource.hpp>
#include <agrpc/grpcContext.hpp>
#include <agrpc/grpcExecutor.hpp>
#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#include <atomic>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct AlwaysFalsePredicate
{
    [[nodiscard]] constexpr bool operator()(const agrpc::GrpcContext&) const noexcept { return false; }
};

inline void drain_completion_queue(agrpc::GrpcContext& grpc_context)
{
    while (detail::GrpcContextImplementation::do_one(grpc_context, detail::GrpcContextImplementation::INFINITE_FUTURE,
                                                     detail::InvokeHandler::NO, detail::AlwaysFalsePredicate{}))
    {
        //
    }
}
}  // namespace detail

inline GrpcContext::GrpcContext(std::unique_ptr<grpc::CompletionQueue>&& completion_queue)
    : completion_queue(std::move(completion_queue))
{
}

inline GrpcContext::~GrpcContext()
{
    this->stop();
    this->shutdown.store(true, std::memory_order_relaxed);
    this->completion_queue->Shutdown();
    detail::drain_completion_queue(*this);
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    asio::execution_context::shutdown();
    asio::execution_context::destroy();
#endif
}

inline bool GrpcContext::run() { return detail::GrpcContextImplementation::run(*this); }

inline bool GrpcContext::poll() { return detail::GrpcContextImplementation::poll(*this); }

inline bool GrpcContext::poll_completion_queue()
{
    return detail::GrpcContextImplementation::poll_completion_queue(*this);
}

inline void GrpcContext::stop()
{
    if (!this->stopped.exchange(true, std::memory_order_relaxed) &&
        !detail::GrpcContextImplementation::running_in_this_thread(*this) && this->remote_work_queue.try_mark_active())
    {
        detail::GrpcContextImplementation::trigger_work_alarm(*this);
    }
}

inline void GrpcContext::reset() noexcept { this->stopped.store(false, std::memory_order_relaxed); }

inline bool GrpcContext::is_stopped() const noexcept { return this->stopped.load(std::memory_order_relaxed); }

inline GrpcContext::executor_type GrpcContext::get_executor() noexcept { return GrpcContext::executor_type{*this}; }

inline GrpcContext::executor_type GrpcContext::get_scheduler() noexcept { return GrpcContext::executor_type{*this}; }

inline GrpcContext::allocator_type GrpcContext::get_allocator() noexcept
{
    return GrpcContext::allocator_type{&this->local_resource};
}

inline void GrpcContext::work_started() noexcept { this->outstanding_work.fetch_add(1, std::memory_order_relaxed); }

inline void GrpcContext::work_finished() noexcept
{
    if AGRPC_UNLIKELY (1 == this->outstanding_work.fetch_sub(1, std::memory_order_relaxed))
    {
        this->stop();
    }
}

inline grpc::CompletionQueue* GrpcContext::get_completion_queue() noexcept { return this->completion_queue.get(); }

inline grpc::ServerCompletionQueue* GrpcContext::get_server_completion_queue() noexcept
{
    return static_cast<grpc::ServerCompletionQueue*>(this->completion_queue.get());
}

AGRPC_NAMESPACE_END

#include <agrpc/detail/grpcContextImplementation.ipp>

#endif  // AGRPC_DETAIL_GRPCCONTEXT_IPP
