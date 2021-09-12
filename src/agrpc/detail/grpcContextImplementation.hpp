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

#ifndef AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_HPP

#include "agrpc/detail/grpcCompletionQueueEvent.hpp"
#include "agrpc/detail/grpcContextOperation.hpp"
#include "agrpc/detail/grpcExecutorOperation.hpp"

namespace agrpc
{
class GrpcContext;

namespace detail
{
struct GrpcContextImplementation
{
    static constexpr void* HAS_WORK_TAG = nullptr;

    static void trigger_work_alarm(agrpc::GrpcContext& grpc_context);

    static void add_remote_work(agrpc::GrpcContext& grpc_context, detail::GrpcContextOperation* op);

    static void add_local_work(agrpc::GrpcContext& grpc_context, detail::GrpcContextOperation* op);

    [[nodiscard]] static bool running_in_this_thread(const agrpc::GrpcContext& grpc_context) noexcept;

    template <detail::GrpcContextOperation::InvokeHandler Invoke>
    static void process_local_queue(agrpc::GrpcContext& grpc_context);

    template <detail::GrpcContextOperation::InvokeHandler Invoke>
    static void process_work(agrpc::GrpcContext& grpc_context, detail::GrpcCompletionQueueEvent event);
};
}  // namespace detail
}  // namespace agrpc

#endif  // AGRPC_DETAIL_GRPCCONTEXTIMPLEMENTATION_HPP
