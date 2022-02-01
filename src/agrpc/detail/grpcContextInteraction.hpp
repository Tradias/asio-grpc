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
template <bool IsIntrusivelyListable, class Handler, class Signature, class... ExtraArgs>
struct AllocateOperationFn
{
    template <class Allocator, class... Args>
    auto operator()(Allocator allocator, Args&&... args) const
    {
        using Operation = detail::Operation<IsIntrusivelyListable, Handler, Allocator, Signature, ExtraArgs...>;
        return detail::allocate<Operation>(allocator, allocator, std::forward<Args>(args)...);
    }

    template <class Allocator, class... Args>
    auto operator()(const agrpc::GrpcContext&, Allocator allocator, Args&&... args) const
    {
        return detail::AllocateOperationFn<IsIntrusivelyListable, Handler, Signature,
                                           detail::GrpcContextLocalAllocator>{}(allocator, std::forward<Args>(args)...);
    }

    template <class T, class... Args>
    auto operator()(agrpc::GrpcContext& grpc_context, std::allocator<T>, Args&&... args) const
    {
        using Operation = detail::LocalOperation<IsIntrusivelyListable, Handler, Signature>;
        return detail::allocate<Operation>(grpc_context.get_allocator(), std::forward<Args>(args)...);
    }
};

template <bool IsIntrusivelyListable, class Handler, class Signature, class... ExtraArgs>
inline constexpr detail::AllocateOperationFn<IsIntrusivelyListable, Handler, Signature, ExtraArgs...>
    allocate_operation{};

template <bool IsBlockingNever, class Handler, class OnLocalOperation, class OnRemoteOperation, class WorkAllocator,
          class... Args>
void create_no_arg_operation(agrpc::GrpcContext& grpc_context, [[maybe_unused]] OnLocalOperation on_local_operation,
                             OnRemoteOperation on_remote_operation, WorkAllocator work_allocator, Args&&... args)
{
    using DecayedHandler = std::decay_t<Handler>;
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        if constexpr (IsBlockingNever)
        {
            auto operation = detail::allocate_operation<true, DecayedHandler, void()>(grpc_context, work_allocator,
                                                                                      std::forward<Args>(args)...);
            grpc_context.work_started();
            detail::WorkFinishedOnExit on_exit{grpc_context};
            on_local_operation(grpc_context, operation.get());
            operation.release();
            on_exit.release();
        }
        else
        {
            DecayedHandler temp{std::forward<Args>(args)...};
            temp();
        }
    }
    else
    {
        auto operation = detail::allocate_operation<true, DecayedHandler, void(), detail::GrpcContextLocalAllocator>(
            work_allocator, std::forward<Args>(args)...);
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        on_remote_operation(grpc_context, operation.get());
        operation.release();
        on_exit.release();
    }
}

template <bool IsBlockingNever, class Handler, class WorkAllocator>
bool create_and_submit_no_arg_operation_if_not_stopped(agrpc::GrpcContext& grpc_context, Handler&& handler,
                                                       WorkAllocator work_allocator)
{
    if AGRPC_UNLIKELY (grpc_context.is_stopped())
    {
        return false;
    }
    detail::create_no_arg_operation<IsBlockingNever, Handler>(
        grpc_context, &detail::GrpcContextImplementation::add_local_operation,
        &detail::GrpcContextImplementation::add_remote_operation, work_allocator, std::forward<Handler>(handler));
    return true;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
