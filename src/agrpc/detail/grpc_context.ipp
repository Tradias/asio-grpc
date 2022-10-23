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

#ifndef AGRPC_DETAIL_GRPC_CONTEXT_IPP
#define AGRPC_DETAIL_GRPC_CONTEXT_IPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_completion_queue_event.hpp>
#include <agrpc/detail/grpc_context.hpp>
#include <agrpc/detail/grpc_executor_options.hpp>
#include <agrpc/detail/intrusive_queue.hpp>
#include <agrpc/detail/memory_resource.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
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

inline grpc::CompletionQueue* get_completion_queue(agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.get_completion_queue();
}
}  // namespace detail

inline GrpcContext::GrpcContext(std::unique_ptr<grpc::CompletionQueue>&& completion_queue)
    : completion_queue(static_cast<std::unique_ptr<grpc::CompletionQueue>&&>(completion_queue))
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
    detail::GrpcContextImplementation::deallocate_async_notify_when_done_list(*this);
}

inline bool GrpcContext::run()
{
    return detail::GrpcContextImplementation::process_work(*this,
                                                           [](agrpc::GrpcContext& grpc_context)
                                                           {
                                                               return detail::GrpcContextImplementation::do_one(
                                                                   grpc_context,
                                                                   detail::GrpcContextImplementation::INFINITE_FUTURE);
                                                           });
}

inline bool GrpcContext::run_completion_queue()
{
    return detail::GrpcContextImplementation::process_work(
        *this,
        [](agrpc::GrpcContext& grpc_context)
        {
            return detail::GrpcContextImplementation::do_one_completion_queue(
                grpc_context, detail::GrpcContextImplementation::INFINITE_FUTURE);
        });
}

inline bool GrpcContext::poll()
{
    return detail::GrpcContextImplementation::process_work(*this,
                                                           [](agrpc::GrpcContext& grpc_context)
                                                           {
                                                               return detail::GrpcContextImplementation::do_one(
                                                                   grpc_context,
                                                                   detail::GrpcContextImplementation::TIME_ZERO);
                                                           });
}

inline bool GrpcContext::run_until_impl(::gpr_timespec deadline)
{
    return detail::GrpcContextImplementation::process_work(*this,
                                                           [deadline](agrpc::GrpcContext& grpc_context)
                                                           {
                                                               return detail::GrpcContextImplementation::do_one(
                                                                   grpc_context, deadline);
                                                           });
}

template <class Condition>
inline bool GrpcContext::run_while(Condition&& condition)
{
    return detail::GrpcContextImplementation::process_work(
        *this,
        [&](agrpc::GrpcContext& grpc_context)
        {
            return condition() && detail::GrpcContextImplementation::do_one(
                                      grpc_context, detail::GrpcContextImplementation::INFINITE_FUTURE);
        });
}

inline bool GrpcContext::poll_completion_queue()
{
    return detail::GrpcContextImplementation::process_work(
        *this,
        [](agrpc::GrpcContext& grpc_context)
        {
            return detail::GrpcContextImplementation::do_one_completion_queue(
                grpc_context, detail::GrpcContextImplementation::TIME_ZERO);
        });
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

#include <agrpc/detail/grpc_context_implementation.ipp>

#endif  // AGRPC_DETAIL_GRPC_CONTEXT_IPP
