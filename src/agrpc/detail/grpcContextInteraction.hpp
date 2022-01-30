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

#ifndef AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/operation.hpp"
#include "agrpc/grpcContext.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <bool IsIntrusivelyListable, class Signature, class... ExtraArgs>
struct AllocateOperationFn
{
    template <class Function, class Allocator>
    auto operator()(Function&& function, Allocator allocator) const
    {
        using Operation =
            detail::Operation<IsIntrusivelyListable, std::decay_t<Function>, Allocator, Signature, ExtraArgs...>;
        return detail::allocate<Operation>(allocator, std::forward<Function>(function), allocator);
    }

    template <class Function, class Allocator>
    auto operator()(const agrpc::GrpcContext&, Function&& function, Allocator allocator) const
    {
        return detail::AllocateOperationFn<IsIntrusivelyListable, Signature, detail::GrpcContextLocalAllocator>{}(
            std::forward<Function>(function), allocator);
    }

    template <class Function, class T>
    auto operator()(agrpc::GrpcContext& grpc_context, Function&& function, std::allocator<T>) const
    {
        using Operation = detail::LocalOperation<IsIntrusivelyListable, std::decay_t<Function>, Signature>;
        return detail::allocate<Operation>(grpc_context.get_allocator(), std::forward<Function>(function));
    }
};

template <bool IsIntrusivelyListable, class Signature, class... ExtraArgs>
inline constexpr detail::AllocateOperationFn<IsIntrusivelyListable, Signature, ExtraArgs...> allocate_operation{};

template <bool IsBlockingNever, class Function, class OnLocalOperation, class OnRemoteOperation, class WorkAllocator>
void create_no_arg_operation(agrpc::GrpcContext& grpc_context, Function&& function, OnLocalOperation on_local_operation,
                             OnRemoteOperation on_remote_operation, WorkAllocator work_allocator)
{
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        if constexpr (IsBlockingNever)
        {
            auto operation = detail::allocate_operation<true, void()>(grpc_context, std::forward<Function>(function),
                                                                      work_allocator);
            grpc_context.work_started();
            detail::WorkFinishedOnExit on_exit{grpc_context};
            on_local_operation(grpc_context, operation.get());
            operation.release();
            on_exit.release();
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
        auto operation = detail::allocate_operation<true, void(), detail::GrpcContextLocalAllocator>(
            std::forward<Function>(function), work_allocator);
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        on_remote_operation(grpc_context, operation.get());
        operation.release();
        on_exit.release();
    }
}

template <bool IsBlockingNever, class Function, class WorkAllocator>
bool create_and_submit_no_arg_operation_if_not_stopped(agrpc::GrpcContext& grpc_context, Function&& function,
                                                       WorkAllocator work_allocator)
{
    if AGRPC_UNLIKELY (grpc_context.is_stopped())
    {
        return false;
    }
    detail::create_no_arg_operation<IsBlockingNever>(
        grpc_context, std::forward<Function>(function), &detail::GrpcContextImplementation::add_local_operation,
        &detail::GrpcContextImplementation::add_remote_operation, work_allocator);
    return true;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
