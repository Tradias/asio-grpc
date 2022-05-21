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

#ifndef AGRPC_DETAIL_ALLOCATEOPERATION_HPP
#define AGRPC_DETAIL_ALLOCATEOPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpcContextImplementation.hpp>
#include <agrpc/detail/operation.hpp>
#include <agrpc/grpcContext.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Handler, class Signature>
using AllocateOperationTemplateArgs = void (*)(Handler, Signature);

template <bool IsIntrusivelyListable>
struct AllocateOperationFn
{
    AllocateOperationFn() = default;
    AllocateOperationFn(const AllocateOperationFn&) = delete;
    AllocateOperationFn(AllocateOperationFn&&) = delete;
    AllocateOperationFn& operator=(const AllocateOperationFn&) = delete;
    AllocateOperationFn& operator=(AllocateOperationFn&&) = delete;

    template <class Handler, class Signature, class Allocator, class... Args>
    auto operator()(const detail::AllocateOperationTemplateArgs<Handler, Signature>, Allocator allocator,
                    Args&&... args) const
    {
        using Operation = detail::Operation<IsIntrusivelyListable, Handler, Allocator, Signature>;
        return detail::allocate<Operation>(allocator, allocator, std::forward<Args>(args)...);
    }

    template <class Handler, class Signature, class Allocator, class... Args>
    auto operator()(const detail::AllocateOperationTemplateArgs<Handler, Signature> t, const agrpc::GrpcContext&,
                    Allocator allocator, Args&&... args) const
    {
        return this->operator()(t, allocator, std::forward<Args>(args)...);
    }

    template <class Handler, class Signature, class T, class... Args>
    auto operator()(const detail::AllocateOperationTemplateArgs<Handler, Signature>, agrpc::GrpcContext& grpc_context,
                    std::allocator<T>, Args&&... args) const
    {
        using Operation = detail::LocalOperation<IsIntrusivelyListable, Handler, Signature>;
        return detail::allocate<Operation>(grpc_context.get_allocator(), std::forward<Args>(args)...);
    }
};

template <bool IsIntrusivelyListable>
inline constexpr detail::AllocateOperationFn<IsIntrusivelyListable> allocate_operation{};

template <bool IsIntrusivelyListable, class Handler, class Signature, class OnLocalOperation, class OnRemoteOperation,
          class WorkAllocator, class... Args>
void allocate_operation_and_invoke(agrpc::GrpcContext& grpc_context, bool is_running_in_this_thread,
                                   OnLocalOperation&& on_local_operation, OnRemoteOperation&& on_remote_operation,
                                   WorkAllocator work_allocator, Args&&... args)
{
    using DecayedHandler = std::decay_t<Handler>;
    grpc_context.work_started();
    detail::WorkFinishedOnExit on_exit{grpc_context};
    if (is_running_in_this_thread)
    {
        auto operation = detail::allocate_operation<IsIntrusivelyListable>(
            detail::AllocateOperationTemplateArgs<DecayedHandler, Signature>{}, grpc_context, work_allocator,
            std::forward<Args>(args)...);
        std::forward<OnLocalOperation>(on_local_operation)(grpc_context, operation.get());
        operation.release();
    }
    else
    {
        auto operation = detail::allocate_operation<IsIntrusivelyListable>(
            detail::AllocateOperationTemplateArgs<DecayedHandler, Signature>{}, work_allocator,
            std::forward<Args>(args)...);
        std::forward<OnRemoteOperation>(on_remote_operation)(grpc_context, operation.get());
        operation.release();
    }
    on_exit.release();
}

template <bool IsIntrusivelyListable, class Handler, class Signature, class OnLocalOperation, class OnRemoteOperation,
          class WorkAllocator, class... Args>
void allocate_operation_and_invoke(agrpc::GrpcContext& grpc_context, OnLocalOperation&& on_local_operation,
                                   OnRemoteOperation&& on_remote_operation, WorkAllocator work_allocator,
                                   Args&&... args)
{
    detail::allocate_operation_and_invoke<IsIntrusivelyListable, Handler, Signature>(
        grpc_context, detail::GrpcContextImplementation::running_in_this_thread(grpc_context),
        std::forward<OnLocalOperation>(on_local_operation), std::forward<OnRemoteOperation>(on_remote_operation),
        work_allocator, std::forward<Args>(args)...);
}

template <bool IsBlockingNever, class Handler, class WorkAllocator>
void create_and_submit_no_arg_operation(agrpc::GrpcContext& grpc_context, Handler&& handler,
                                        WorkAllocator work_allocator)
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
            std::decay_t<Handler> temp{std::forward<Handler>(handler)};
            temp();
            return;
        }
    }
    detail::allocate_operation_and_invoke<true, Handler, void()>(
        grpc_context, is_running_in_this_thread, &detail::GrpcContextImplementation::add_local_operation,
        &detail::GrpcContextImplementation::add_remote_operation, work_allocator, std::forward<Handler>(handler));
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATEOPERATION_HPP
