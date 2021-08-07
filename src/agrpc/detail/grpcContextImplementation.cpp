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

#include "agrpc/detail/grpcContextImplementation.hpp"

#include "agrpc/detail/grpcContextOperation.hpp"
#include "agrpc/detail/grpcExecutorOperation.hpp"
#include "agrpc/grpcContext.hpp"

#include <atomic>
#include <cstdint>
#include <limits>

namespace agrpc::detail
{
void GrpcContextImplementation::trigger_work_alarm(agrpc::GrpcContext& grpc_context)
{
    static constexpr ::gpr_timespec TIME_ZERO{std::numeric_limits<std::int64_t>::min(), 0, ::GPR_CLOCK_MONOTONIC};
    if (!grpc_context.has_work.exchange(true, std::memory_order_acquire))
    {
        grpc_context.work_alarm.Set(grpc_context.completion_queue.get(), TIME_ZERO,
                                    detail::GrpcContextImplementation::HAS_WORK_TAG);
    }
}

void GrpcContextImplementation::add_remote_work(agrpc::GrpcContext& grpc_context, detail::GrpcContextOperation* op)
{
    grpc_context.remote_work_queue.push(op);
    GrpcContextImplementation::trigger_work_alarm(grpc_context);
}

void GrpcContextImplementation::add_local_work(agrpc::GrpcContext& grpc_context, detail::GrpcContextOperation* op)
{
    grpc_context.local_work_queue.push_back(*op);
    if (!grpc_context.is_processing_local_work)
    {
        GrpcContextImplementation::trigger_work_alarm(grpc_context);
    }
}

[[nodiscard]] bool GrpcContextImplementation::running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept
{
    return grpc_context.thread_id.load(std::memory_order_relaxed) == std::this_thread::get_id();
}

template <detail::GrpcContextOperation::InvokeHandler Invoke>
void GrpcContextImplementation::process_local_queue(agrpc::GrpcContext& grpc_context, bool ok)
{
    while (!grpc_context.local_work_queue.empty())
    {
        grpc_context.is_processing_local_work = true;
        auto& operation = grpc_context.local_work_queue.front();
        grpc_context.local_work_queue.pop_front();
        operation.complete(ok, Invoke);
    }
    grpc_context.is_processing_local_work = false;
}

template void GrpcContextImplementation::process_local_queue<detail::GrpcContextOperation::InvokeHandler::YES>(
    agrpc::GrpcContext& grpc_context, bool ok);
template void GrpcContextImplementation::process_local_queue<detail::GrpcContextOperation::InvokeHandler::NO>(
    agrpc::GrpcContext& grpc_context, bool ok);

template <detail::GrpcContextOperation::InvokeHandler Invoke>
void GrpcContextImplementation::process_work(agrpc::GrpcContext& grpc_context,
                                             const detail::GrpcCompletionQueueEvent& event)
{
    if (event.tag == detail::GrpcContextImplementation::HAS_WORK_TAG)
    {
        grpc_context.has_work.store(false, std::memory_order_release);
        process_local_queue<Invoke>(grpc_context, event.ok);
        grpc_context.remote_work_queue.consume_all(
            [&](auto* operation)
            {
                operation->complete(event.ok, Invoke);
            });
    }
    else
    {
        auto* operation = static_cast<detail::GrpcContextOperation*>(event.tag);
        operation->complete(event.ok, Invoke);
    }
}
template void GrpcContextImplementation::process_work<detail::GrpcContextOperation::InvokeHandler::YES>(
    agrpc::GrpcContext& grpc_context, const detail::GrpcCompletionQueueEvent& event);
template void GrpcContextImplementation::process_work<detail::GrpcContextOperation::InvokeHandler::NO>(
    agrpc::GrpcContext& grpc_context, const detail::GrpcCompletionQueueEvent& event);
}  // namespace agrpc::detail
