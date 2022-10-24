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

#ifndef AGRPC_DETAIL_GRPC_SUBMIT_HPP
#define AGRPC_DETAIL_GRPC_SUBMIT_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct DeallocateOperationFunctor
{
    agrpc::GrpcContext& grpc_context;
    detail::TypeErasedGrpcTagOperation* operation;

    void operator()() { operation->complete(detail::InvokeHandler::NO, false, grpc_context); }
};

using OperationAllocationGuard = detail::ScopeGuard<detail::DeallocateOperationFunctor>;

template <class InitiatingFunction, class CompletionHandler>
void grpc_submit(agrpc::GrpcContext& grpc_context, InitiatingFunction& initiating_function,
                 CompletionHandler&& completion_handler)
{
    auto operation = detail::allocate_operation<detail::GrpcTagOperation>(
        grpc_context, static_cast<CompletionHandler&&>(completion_handler));
    detail::OperationAllocationGuard allocation_guard{grpc_context, operation};
    detail::StartWorkAndGuard start_work_guard{grpc_context};
    initiating_function(grpc_context, operation);
    start_work_guard.release();
    allocation_guard.release();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SUBMIT_HPP
