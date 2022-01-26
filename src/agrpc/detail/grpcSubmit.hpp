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

#ifndef AGRPC_DETAIL_GRPCSUBMIT_HPP
#define AGRPC_DETAIL_GRPCSUBMIT_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/grpcContextInteraction.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class InitiatingFunction, class CompletionHandler, class Allocator>
void grpc_submit(agrpc::GrpcContext& grpc_context, InitiatingFunction initiating_function,
                 CompletionHandler completion_handler, Allocator allocator)
{
    grpc_context.work_started();
    detail::WorkFinishedOnExit on_exit{grpc_context};
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        auto operation =
            detail::allocate_operation<false, void(bool)>(grpc_context, std::move(completion_handler), allocator);
        std::move(initiating_function)(grpc_context, operation.get());
        operation.release();
    }
    else
    {
        auto operation = detail::allocate_operation<false, void(bool), detail::GrpcContextLocalAllocator>(
            std::move(completion_handler), allocator);
        std::move(initiating_function)(grpc_context, operation.get());
        operation.release();
    }
    on_exit.release();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCSUBMIT_HPP
