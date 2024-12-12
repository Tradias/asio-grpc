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

#ifndef AGRPC_DETAIL_GRPC_CONTEXT_DEFINITION_HPP
#define AGRPC_DETAIL_GRPC_CONTEXT_DEFINITION_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/grpc_completion_queue_event.hpp>
#include <agrpc/detail/grpc_executor_options.hpp>
#include <agrpc/detail/intrusive_queue.hpp>
#include <agrpc/grpc_context.hpp>
#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#include <atomic>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Function>
struct GrpcContextLoopCondition : Function
{
    static constexpr bool COMPLETION_QUEUE_ONLY = false;

    using Function::operator();

    [[nodiscard]] bool has_processed(detail::DoOneResult result) const noexcept { return bool{result}; }
};

template <class Function>
GrpcContextLoopCondition(Function) -> GrpcContextLoopCondition<Function>;

template <class Function>
struct GrpcContextCompletionQueueLoopCondition : Function
{
    static constexpr bool COMPLETION_QUEUE_ONLY = true;

    using Function::operator();

    [[nodiscard]] bool has_processed(detail::DoOneResult result) const noexcept
    {
        return result.handled_event() && !result.check_remote_work();
    }
};

template <class Function>
GrpcContextCompletionQueueLoopCondition(Function) -> GrpcContextCompletionQueueLoopCondition<Function>;

struct GrpcContextIsNotStopped
{
    [[nodiscard]] bool operator()() const noexcept { return !grpc_context_.is_stopped(); }

    agrpc::GrpcContext& grpc_context_;
};

template <class T>
inline void create_resources(T& resources, std::size_t concurrency_hint)
{
    for (size_t i{}; i != concurrency_hint; ++i)
    {
        auto resource = new detail::ListablePoolResource();
        resources.push_front(*resource);
    }
}

template <class T>
inline void delete_resources(T& resources)
{
    while (!resources.empty())
    {
        delete &resources.pop_front();
    }
}
}  // namespace detail

inline GrpcContext::GrpcContext() : GrpcContext(std::make_unique<grpc::CompletionQueue>(), 1) {}

inline GrpcContext::GrpcContext(std::size_t concurrency_hint)
    : GrpcContext(std::make_unique<grpc::CompletionQueue>(), concurrency_hint)
{
}

template <class>
inline GrpcContext::GrpcContext(std::unique_ptr<grpc::CompletionQueue>&& completion_queue)
    : GrpcContext(static_cast<std::unique_ptr<grpc::CompletionQueue>&&>(completion_queue), 1)
{
}

inline GrpcContext::GrpcContext(std::unique_ptr<grpc::ServerCompletionQueue> completion_queue)
    : GrpcContext(static_cast<std::unique_ptr<grpc::ServerCompletionQueue>&&>(completion_queue), 1)
{
}

inline GrpcContext::GrpcContext(std::unique_ptr<grpc::ServerCompletionQueue> completion_queue,
                                std::size_t concurrency_hint)
    : multithreaded_{concurrency_hint > 1},
      completion_queue_(static_cast<std::unique_ptr<grpc::ServerCompletionQueue>&&>(completion_queue))

{
    detail::create_resources(memory_resources_, concurrency_hint);
}

inline GrpcContext::GrpcContext(std::unique_ptr<grpc::CompletionQueue> completion_queue, std::size_t concurrency_hint)
    : multithreaded_{concurrency_hint > 1},
      completion_queue_(static_cast<std::unique_ptr<grpc::CompletionQueue>&&>(completion_queue))

{
    detail::create_resources(memory_resources_, concurrency_hint);
}

inline GrpcContext::~GrpcContext()
{
    stop();
    shutdown_ = true;
    completion_queue_->Shutdown();
    detail::GrpcContextImplementation::drain_completion_queue(*this);
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    asio::execution_context::shutdown();
    asio::execution_context::destroy();
#endif
    detail::delete_resources(memory_resources_);
}

inline bool GrpcContext::run()
{
    return detail::GrpcContextImplementation::process_work(
        *this, detail::GrpcContextLoopCondition{detail::GrpcContextIsNotStopped{*this}},
        detail::GrpcContextImplementation::INFINITE_FUTURE);
}

inline bool GrpcContext::run_completion_queue()
{
    return detail::GrpcContextImplementation::process_work(
        *this, detail::GrpcContextCompletionQueueLoopCondition{detail::GrpcContextIsNotStopped{*this}},
        detail::GrpcContextImplementation::INFINITE_FUTURE);
}

inline bool GrpcContext::poll()
{
    return detail::GrpcContextImplementation::process_work(
        *this, detail::GrpcContextLoopCondition{detail::GrpcContextIsNotStopped{*this}},
        detail::GrpcContextImplementation::TIME_ZERO);
}

inline bool GrpcContext::run_until_impl(::gpr_timespec deadline)
{
    return detail::GrpcContextImplementation::process_work(
        *this, detail::GrpcContextLoopCondition{detail::GrpcContextIsNotStopped{*this}}, deadline);
}

template <class Condition>
inline bool GrpcContext::run_while(Condition&& condition)
{
    return detail::GrpcContextImplementation::process_work(*this,
                                                           detail::GrpcContextLoopCondition{[&]
                                                                                            {
                                                                                                return condition() &&
                                                                                                       !is_stopped();
                                                                                            }},
                                                           detail::GrpcContextImplementation::INFINITE_FUTURE);
}

inline bool GrpcContext::poll_completion_queue()
{
    return detail::GrpcContextImplementation::process_work(
        *this, detail::GrpcContextCompletionQueueLoopCondition{detail::GrpcContextIsNotStopped{*this}},
        detail::GrpcContextImplementation::TIME_ZERO);
}

inline void GrpcContext::stop()
{
    if (!stopped_.exchange(true, std::memory_order_relaxed) &&
        !detail::GrpcContextImplementation::running_in_this_thread(*this) && remote_work_queue_.try_mark_active())
    {
        detail::GrpcContextImplementation::trigger_work_alarm(*this);
    }
}

inline void GrpcContext::reset() noexcept { stopped_.store(false, std::memory_order_relaxed); }

inline bool GrpcContext::is_stopped() const noexcept { return stopped_.load(std::memory_order_relaxed); }

inline GrpcContext::executor_type GrpcContext::get_executor() noexcept { return GrpcContext::executor_type{*this}; }

inline GrpcContext::executor_type GrpcContext::get_scheduler() noexcept { return GrpcContext::executor_type{*this}; }

inline GrpcContext::allocator_type GrpcContext::get_allocator() noexcept { return allocator_type{}; }

inline void GrpcContext::work_started() noexcept { outstanding_work_.fetch_add(1, std::memory_order_relaxed); }

inline void GrpcContext::work_finished() noexcept
{
    if AGRPC_UNLIKELY (1 == outstanding_work_.fetch_sub(1, std::memory_order_relaxed))
    {
        stop();
    }
}

inline grpc::CompletionQueue* GrpcContext::get_completion_queue() noexcept { return completion_queue_.get(); }

inline grpc::ServerCompletionQueue* GrpcContext::get_server_completion_queue() noexcept
{
    return static_cast<grpc::ServerCompletionQueue*>(completion_queue_.get());
}

#ifdef AGRPC_STDEXEC
namespace detail
{
template <class Tag>
agrpc::GrpcContext::executor_type tag_invoke(stdexec::get_completion_scheduler_t<Tag>, const BasicSenderEnv& e) noexcept
{
    return e.grpc_context_.get_scheduler();
}
}
#endif

AGRPC_NAMESPACE_END

#include <agrpc/detail/grpc_context_implementation_definition.hpp>

#endif  // AGRPC_DETAIL_GRPC_CONTEXT_DEFINITION_HPP
