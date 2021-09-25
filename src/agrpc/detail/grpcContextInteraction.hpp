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

#ifndef AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP

#include "agrpc/detail/attributes.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/operation.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc::detail
{
template <bool IsIntrusivelyListable, class... Signature>
struct AllocateOperationAndInvoke
{
    template <class Function, class OnOperation, class Allocator>
    void operator()(Function&& function, OnOperation&& on_operation, Allocator allocator) const
    {
        using Operation = detail::Operation<IsIntrusivelyListable, std::decay_t<Function>, Allocator, Signature...>;
        auto ptr = detail::allocate<Operation>(allocator, std::forward<Function>(function), allocator);
        std::forward<OnOperation>(on_operation)(ptr.get());
        ptr.release();
    }

    template <class Function, class OnOperation, class Allocator>
    void operator()(agrpc::GrpcContext&, Function&& function, OnOperation&& on_operation, Allocator allocator) const
    {
        detail::AllocateOperationAndInvoke<IsIntrusivelyListable, Signature..., detail::GrpcContextLocalAllocator>{}(
            std::forward<Function>(function), std::forward<OnOperation>(on_operation), allocator);
    }

    template <class Function, class OnOperation, class T>
    void operator()(agrpc::GrpcContext& grpc_context, Function&& function, OnOperation&& on_operation,
                    std::allocator<T>) const
    {
        using Operation = detail::LocalOperation<IsIntrusivelyListable, std::decay_t<Function>, Signature...>;
        auto ptr = detail::allocate<Operation>(grpc_context.get_allocator(), std::forward<Function>(function));
        std::forward<OnOperation>(on_operation)(ptr.get());
        ptr.release();
    }
};

template <bool IsIntrusivelyListable, class... Signature>
inline constexpr detail::AllocateOperationAndInvoke<IsIntrusivelyListable, Signature...>
    allocate_operation_and_invoke{};

struct AddToLocalOperations
{
    agrpc::GrpcContext& grpc_context;

    void operator()(detail::TypeErasedNoArgLocalOperation* op)
    {
        detail::GrpcContextImplementation::add_local_operation(grpc_context, op);
    }
};

struct AddToRemoteOperations
{
    agrpc::GrpcContext& grpc_context;

    void operator()(detail::TypeErasedNoArgRemoteOperation* op)
    {
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, op);
    }
};

template <bool IsBlockingNever, class Function, class WorkAllocator>
void create_no_arg_operation(agrpc::GrpcContext& grpc_context, Function&& function, WorkAllocator work_allocator)
{
    if (grpc_context.is_stopped()) AGRPC_UNLIKELY
        {
            return;
        }
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        if constexpr (IsBlockingNever)
        {
            detail::allocate_operation_and_invoke<true>(grpc_context, std::forward<Function>(function),
                                                        detail::AddToLocalOperations{grpc_context}, work_allocator);
        }
        else
        {
            using DecayedFunction = std::decay_t<Function>;
            DecayedFunction temp{std::forward<Function>(function)};
            temp();
        }
    }
    else
    {
        detail::allocate_operation_and_invoke<false>(std::forward<Function>(function),
                                                     detail::AddToRemoteOperations{grpc_context}, work_allocator);
    }
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
