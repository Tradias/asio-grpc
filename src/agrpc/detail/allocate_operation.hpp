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

#ifndef AGRPC_DETAIL_ALLOCATE_OPERATION_HPP
#define AGRPC_DETAIL_ALLOCATE_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/operation.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
inline constexpr bool IS_STD_ALLOCATOR = false;

template <class T>
inline constexpr bool IS_STD_ALLOCATOR<std::allocator<T>> = true;

template <template <class> class OperationTemplate, class Operation, class... Args>
auto allocate_operation(Operation&& operation, Args&&... args)

{
    const auto allocator = detail::exec::get_allocator(operation);
    return detail::allocate<OperationTemplate<detail::RemoveCrefT<Operation>>>(
        allocator, static_cast<Operation&&>(operation), static_cast<Args&&>(args)...);
}

template <class AllocationTraits, class Operation, class... Args>
auto allocate_local_operation(agrpc::GrpcContext& grpc_context, Operation&& operation, Args&&... args)

{
    using DecayedOperation = detail::RemoveCrefT<Operation>;
    auto allocator = detail::exec::get_allocator(operation);
    if constexpr (detail::IS_STD_ALLOCATOR<decltype(allocator)>)
    {
        return detail::allocate<typename AllocationTraits::template Local<DecayedOperation>>(
            grpc_context.get_allocator(), static_cast<Operation&&>(operation), static_cast<Args&&>(args)...);
    }
    else
    {
        return detail::allocate<typename AllocationTraits::template Custom<DecayedOperation>>(
            allocator, static_cast<Operation&&>(operation), static_cast<Args&&>(args)...);
    }
}

template <class AllocationTraits, class OnOperation, class Operation, class... Args>
void allocate_operation_and_invoke(agrpc::GrpcContext& grpc_context, OnOperation& on_operation, Operation&& operation,
                                   Args&&... args)
{
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        auto allocated_operation = detail::allocate_local_operation<AllocationTraits>(
            grpc_context, static_cast<Operation&&>(operation), static_cast<Args&&>(args)...);
        on_operation(grpc_context, allocated_operation.get());
        allocated_operation.release();
    }
    else
    {
        auto allocated_operation = detail::allocate_operation<AllocationTraits::template Custom>(
            static_cast<Operation&&>(operation), static_cast<Args&&>(args)...);
        on_operation(grpc_context, allocated_operation.get());
        allocated_operation.release();
    }
}

struct NoArgOperationAllocationTraits
{
    template <class Operation>
    using Local = detail::LocalOperation<true, Operation, void()>;

    template <class Operation>
    using Custom = detail::Operation<true, Operation, void()>;
};

template <bool IsBlockingNever, class Operation>
void create_and_submit_no_arg_operation(agrpc::GrpcContext& grpc_context, Operation&& operation)
{
    if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
    {
        return;
    }
    const auto is_running_in_this_thread = detail::GrpcContextImplementation::running_in_this_thread(grpc_context);
    if constexpr (!IsBlockingNever)
    {
        if (is_running_in_this_thread)
        {
            auto op{static_cast<Operation&&>(operation)};
            std::move(op)();
            return;
        }
    }
    detail::StartWorkAndGuard guard{grpc_context};
    if (is_running_in_this_thread)
    {
        auto allocated_operation = detail::allocate_local_operation<NoArgOperationAllocationTraits>(
            grpc_context, static_cast<Operation&&>(operation));
        detail::GrpcContextImplementation::add_local_operation(grpc_context, allocated_operation.release());
    }
    else
    {
        auto allocated_operation = detail::allocate_operation<NoArgOperationAllocationTraits::template Custom>(
            static_cast<Operation&&>(operation));
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, allocated_operation.release());
    }
    guard.release();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_OPERATION_HPP
