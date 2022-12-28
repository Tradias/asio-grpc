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

#ifndef AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_completion_queue_event.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/detail/utility.hpp>
#include <grpcpp/completion_queue.h>

#include <cstdint>
#include <limits>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct WorkFinishedOnExitFunctor
{
    agrpc::GrpcContext& grpc_context_;

    explicit WorkFinishedOnExitFunctor(agrpc::GrpcContext& grpc_context) noexcept : grpc_context_(grpc_context) {}

    void operator()() const noexcept;

    WorkFinishedOnExitFunctor(const WorkFinishedOnExitFunctor&) = delete;
    WorkFinishedOnExitFunctor(WorkFinishedOnExitFunctor&&) = delete;
    WorkFinishedOnExitFunctor& operator=(const WorkFinishedOnExitFunctor&) = delete;
    WorkFinishedOnExitFunctor& operator=(WorkFinishedOnExitFunctor&&) = delete;
};

using WorkFinishedOnExit = detail::ScopeGuard<detail::WorkFinishedOnExitFunctor>;

struct StartWorkAndGuard : detail::WorkFinishedOnExit
{
    explicit StartWorkAndGuard(agrpc::GrpcContext& grpc_context) noexcept;

    StartWorkAndGuard(const StartWorkAndGuard&) = delete;
    StartWorkAndGuard(StartWorkAndGuard&&) = delete;
    StartWorkAndGuard& operator=(const StartWorkAndGuard&) = delete;
    StartWorkAndGuard& operator=(StartWorkAndGuard&&) = delete;
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct GrpcContextThreadInfo : asio::detail::thread_info_base
{
};

// Enables Boost.Asio's awaitable frame memory recycling
struct GrpcContextThreadContext : asio::detail::thread_context
{
    detail::GrpcContextThreadInfo this_thread_;
    thread_call_stack::context ctx_{this, this_thread_};
};
#else
struct GrpcContextThreadContext
{
};
#endif

struct ThreadLocalGrpcContextGuard
{
    const agrpc::GrpcContext* old_context_;

    explicit ThreadLocalGrpcContextGuard(const agrpc::GrpcContext& grpc_context) noexcept;

    ~ThreadLocalGrpcContextGuard();

    ThreadLocalGrpcContextGuard(const ThreadLocalGrpcContextGuard&) = delete;
    ThreadLocalGrpcContextGuard(ThreadLocalGrpcContextGuard&&) = delete;
    ThreadLocalGrpcContextGuard& operator=(const ThreadLocalGrpcContextGuard&) = delete;
    ThreadLocalGrpcContextGuard& operator=(ThreadLocalGrpcContextGuard&&) = delete;
};

struct IsGrpcContextStoppedPredicate
{
    [[nodiscard]] bool operator()(const agrpc::GrpcContext& grpc_context) const noexcept;
};

struct GrpcContextImplementation
{
    static constexpr void* HAS_REMOTE_WORK_TAG = nullptr;
    static constexpr ::gpr_timespec TIME_ZERO{std::numeric_limits<std::int64_t>::min(), 0, ::GPR_CLOCK_MONOTONIC};
    static constexpr ::gpr_timespec INFINITE_FUTURE{std::numeric_limits<std::int64_t>::max(), 0, ::GPR_CLOCK_MONOTONIC};

    [[nodiscard]] static bool is_shutdown(const agrpc::GrpcContext& grpc_context) noexcept;

    static void trigger_work_alarm(agrpc::GrpcContext& grpc_context) noexcept;

    static void work_started(agrpc::GrpcContext& grpc_context) noexcept;

    static void add_remote_operation(agrpc::GrpcContext& grpc_context, detail::QueueableOperationBase* op) noexcept;

    static void add_local_operation(agrpc::GrpcContext& grpc_context, detail::QueueableOperationBase* op) noexcept;

    static void add_operation(agrpc::GrpcContext& grpc_context, detail::QueueableOperationBase* op) noexcept;

    static void add_notify_when_done_operation(agrpc::GrpcContext& grpc_context,
                                               detail::NotfiyWhenDoneSenderImplementation* implementation) noexcept;

    static void remove_notify_when_done_operation(agrpc::GrpcContext& grpc_context,
                                                  detail::NotfiyWhenDoneSenderImplementation* implementation) noexcept;

    static void deallocate_notify_when_done_list(agrpc::GrpcContext& grpc_context);

    static bool handle_next_completion_queue_event(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline,
                                                   detail::InvokeHandler invoke);

    [[nodiscard]] static bool running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept;

    static const agrpc::GrpcContext* set_thread_local_grpc_context(const agrpc::GrpcContext* grpc_context) noexcept;

    static bool move_remote_work_to_local_queue(agrpc::GrpcContext& grpc_context) noexcept;

    static bool process_local_queue(agrpc::GrpcContext& grpc_context, detail::InvokeHandler invoke);

    template <class StopPredicate = detail::IsGrpcContextStoppedPredicate>
    static bool do_one(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline,
                       detail::InvokeHandler invoke = detail::InvokeHandler::YES, StopPredicate stop_predicate = {});

    static bool do_one_completion_queue(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline,
                                        detail::InvokeHandler invoke = detail::InvokeHandler::YES);

    template <class LoopFunction>
    static bool process_work(agrpc::GrpcContext& grpc_context, LoopFunction loop_function);
};

void process_grpc_tag(void* tag, detail::OperationResult result, agrpc::GrpcContext& grpc_context);

::gpr_timespec gpr_timespec_from_now(std::chrono::nanoseconds duration) noexcept;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_CONTEXT_IMPLEMENTATION_HPP
