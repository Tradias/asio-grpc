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

#ifndef AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_completion_queue_event.hpp>
#include <agrpc/detail/intrusive_queue.hpp>
#include <agrpc/detail/listable_pool_resource.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/detail/pool_resource.hpp>
#include <agrpc/detail/utility.hpp>
#include <grpcpp/completion_queue.h>

#include <cstdint>
#include <limits>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct WorkFinishedOnExitFunctor
{
    agrpc::GrpcContext& grpc_context_;

    explicit WorkFinishedOnExitFunctor(agrpc::GrpcContext& grpc_context) noexcept : grpc_context_(grpc_context) {}

    void operator()() const noexcept;
};

using WorkFinishedOnExit = detail::ScopeGuard<detail::WorkFinishedOnExitFunctor>;

struct GrpcContextThreadContext
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    // Enables Boost.Asio's awaitable frame memory recycling
    : asio::detail::thread_context
#endif
{
    explicit GrpcContextThreadContext(agrpc::GrpcContext& grpc_context);

    GrpcContextThreadContext(agrpc::GrpcContext& grpc_context, bool multithreaded);

    ~GrpcContextThreadContext() noexcept;

    GrpcContextThreadContext(const GrpcContextThreadContext& other) = delete;
    GrpcContextThreadContext(GrpcContextThreadContext&& other) = delete;
    GrpcContextThreadContext& operator=(const GrpcContextThreadContext& other) = delete;
    GrpcContextThreadContext& operator=(GrpcContextThreadContext&& other) = delete;

    const bool multithreaded_;
    bool check_remote_work_;
    agrpc::GrpcContext& grpc_context_;
    detail::IntrusiveQueue<detail::QueueableOperationBase> local_work_queue_;
    GrpcContextThreadContext* old_context_;
    detail::ListablePoolResource& resource_;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    asio::detail::thread_info_base this_thread_;
    thread_call_stack::context ctx_{this, this_thread_};
#endif
};

enum class InvokeHandler
{
    NO_,
    YES_
};

struct CompletionQueueEventResult
{
    static constexpr uint32_t CHECK_REMOTE_WORK = 1 << 0;
    static constexpr uint32_t HANDLED_EVENT = 1 << 1;

    uint32_t flags_{};

    [[nodiscard]] bool handled_event() const noexcept { return (flags_ & HANDLED_EVENT) != 0; }

    [[nodiscard]] bool check_remote_work() const noexcept { return (flags_ & CHECK_REMOTE_WORK) != 0; }
};

struct DoOneResult : CompletionQueueEventResult
{
    static constexpr uint32_t PROCESSED_LOCAL_WORK = HANDLED_EVENT << 1;

    static DoOneResult from(CompletionQueueEventResult handled_event, bool processed_local_work) noexcept
    {
        return {{handled_event.flags_ | (processed_local_work ? DoOneResult::PROCESSED_LOCAL_WORK : uint32_t{})}};
    }

    explicit operator bool() const noexcept { return processed_local_work() || handled_event(); }

    [[nodiscard]] bool processed_local_work() const noexcept { return (flags_ & PROCESSED_LOCAL_WORK) != 0; }
};

struct GrpcContextImplementation
{
    static constexpr void* CHECK_REMOTE_WORK_TAG = nullptr;
    static constexpr ::gpr_timespec TIME_ZERO{std::numeric_limits<std::int64_t>::min(), 0, ::GPR_CLOCK_MONOTONIC};
    static constexpr ::gpr_timespec INFINITE_FUTURE{std::numeric_limits<std::int64_t>::max(), 0, ::GPR_CLOCK_MONOTONIC};

    [[nodiscard]] static bool is_shutdown(const agrpc::GrpcContext& grpc_context) noexcept;

    static void trigger_work_alarm(agrpc::GrpcContext& grpc_context) noexcept;

    static void work_started(agrpc::GrpcContext& grpc_context) noexcept;

    static void add_remote_operation(agrpc::GrpcContext& grpc_context, detail::QueueableOperationBase* op) noexcept;

    static void add_local_operation(detail::QueueableOperationBase* op) noexcept;

    static void add_operation(agrpc::GrpcContext& grpc_context, detail::QueueableOperationBase* op) noexcept;

    static CompletionQueueEventResult handle_next_completion_queue_event(detail::GrpcContextThreadContext& context,
                                                                         ::gpr_timespec deadline,
                                                                         detail::InvokeHandler invoke);

    [[nodiscard]] static bool running_in_this_thread() noexcept;

    [[nodiscard]] static bool running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept;

    [[nodiscard]] static bool move_local_queue_to_remote_work(detail::GrpcContextThreadContext& context) noexcept;

    [[nodiscard]] static bool move_remote_work_to_local_queue(detail::GrpcContextThreadContext& context) noexcept;

    [[nodiscard]] static bool distribute_all_local_work_to_other_threads_but_one(
        detail::GrpcContextThreadContext& context) noexcept;

    static bool process_local_queue(detail::GrpcContextThreadContext& context, detail::InvokeHandler invoke);

    static DoOneResult do_one(detail::GrpcContextThreadContext& context, ::gpr_timespec deadline,
                              detail::InvokeHandler invoke = detail::InvokeHandler::YES_);

    static DoOneResult do_one_if_not_stopped(detail::GrpcContextThreadContext& context, ::gpr_timespec deadline);

    static DoOneResult do_one_completion_queue(detail::GrpcContextThreadContext& context, ::gpr_timespec deadline);

    static DoOneResult do_one_completion_queue_if_not_stopped(detail::GrpcContextThreadContext& context,
                                                              ::gpr_timespec deadline);

    template <class LoopFunction>
    static bool process_work(agrpc::GrpcContext& grpc_context, LoopFunction loop_function);

    static void drain_completion_queue(agrpc::GrpcContext& grpc_context) noexcept;

    static detail::ListablePoolResource& pop_resource(agrpc::GrpcContext& grpc_context);

    static void push_resource(agrpc::GrpcContext& grpc_context, detail::ListablePoolResource& resource);
};

void process_grpc_tag(void* tag, detail::OperationResult result, agrpc::GrpcContext& grpc_context);

::gpr_timespec gpr_timespec_from_now(std::chrono::nanoseconds duration) noexcept;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_HPP
