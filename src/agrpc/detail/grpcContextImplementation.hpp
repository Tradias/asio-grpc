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

#ifndef AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpcCompletionQueueEvent.hpp>
#include <agrpc/detail/typeErasedOperation.hpp>
#include <agrpc/detail/utility.hpp>
#include <grpcpp/completion_queue.h>

#include <cstdint>
#include <limits>

AGRPC_NAMESPACE_BEGIN()

class GrpcContext;

namespace detail
{
struct WorkFinishedOnExitFunctor
{
    agrpc::GrpcContext& grpc_context;

    explicit WorkFinishedOnExitFunctor(agrpc::GrpcContext& grpc_context) noexcept : grpc_context(grpc_context) {}

    void operator()() const noexcept;

    WorkFinishedOnExitFunctor(const WorkFinishedOnExitFunctor&) = delete;
    WorkFinishedOnExitFunctor(WorkFinishedOnExitFunctor&&) = delete;
    WorkFinishedOnExitFunctor& operator=(const WorkFinishedOnExitFunctor&) = delete;
    WorkFinishedOnExitFunctor& operator=(WorkFinishedOnExitFunctor&&) = delete;
};

struct WorkFinishedOnExit : detail::ScopeGuard<detail::WorkFinishedOnExitFunctor>
{
    explicit WorkFinishedOnExit(agrpc::GrpcContext& grpc_context) noexcept
        : detail::ScopeGuard<detail::WorkFinishedOnExitFunctor>(grpc_context)
    {
    }

    WorkFinishedOnExit(const WorkFinishedOnExit&) = delete;
    WorkFinishedOnExit(WorkFinishedOnExit&&) = delete;
    WorkFinishedOnExit& operator=(const WorkFinishedOnExit&) = delete;
    WorkFinishedOnExit& operator=(WorkFinishedOnExit&&) = delete;
};

struct GrpcContextImplementation
{
    static constexpr void* HAS_REMOTE_WORK_TAG = nullptr;
    static constexpr ::gpr_timespec TIME_ZERO{std::numeric_limits<std::int64_t>::min(), 0, ::GPR_CLOCK_MONOTONIC};
    static constexpr ::gpr_timespec INFINITE_FUTURE{std::numeric_limits<std::int64_t>::max(), 0, ::GPR_CLOCK_MONOTONIC};

    [[nodiscard]] static bool is_shutdown(const agrpc::GrpcContext& grpc_context) noexcept;

    static void trigger_work_alarm(agrpc::GrpcContext& grpc_context) noexcept;

    static void add_remote_operation(agrpc::GrpcContext& grpc_context, detail::TypeErasedNoArgOperation* op) noexcept;

    static void add_local_operation(agrpc::GrpcContext& grpc_context, detail::TypeErasedNoArgOperation* op) noexcept;

    static void add_operation(agrpc::GrpcContext& grpc_context, detail::TypeErasedNoArgOperation* op) noexcept;

    static bool get_next_event(agrpc::GrpcContext& grpc_context, detail::GrpcCompletionQueueEvent& event) noexcept;

    static bool poll_next_event(agrpc::GrpcContext& grpc_context, detail::GrpcCompletionQueueEvent& event) noexcept;

    [[nodiscard]] static bool running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept;

    static const agrpc::GrpcContext* set_thread_local_grpc_context(const agrpc::GrpcContext* grpc_context) noexcept;

    static bool move_remote_work_to_local_queue(agrpc::GrpcContext& grpc_context) noexcept;

    template <detail::InvokeHandler Invoke>
    static void process_local_queue(agrpc::GrpcContext& grpc_context);

    template <detail::InvokeHandler Invoke, class StopCondition>
    static bool process_work(agrpc::GrpcContext& grpc_context, StopCondition stop_condition, ::gpr_timespec deadline);

    static bool process_work(agrpc::GrpcContext& grpc_context, ::gpr_timespec deadline);

    static bool run(agrpc::GrpcContext& grpc_context);

    static bool poll(agrpc::GrpcContext& grpc_context);
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_HPP
