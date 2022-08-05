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

template <bool IsIntrusivelyListable, class Signature, class CompletionHandler>
detail::AllocatedPointerT<detail::Operation<IsIntrusivelyListable, detail::RemoveCrefT<CompletionHandler>, Signature>,
                          detail::AssociatedAllocatorT<CompletionHandler>>
allocate_operation(CompletionHandler&& completion_handler)

{
    using Operation = detail::Operation<IsIntrusivelyListable, detail::RemoveCrefT<CompletionHandler>, Signature>;
    const auto allocator = detail::exec::get_allocator(completion_handler);
    return detail::allocate<Operation>(allocator, std::forward<CompletionHandler>(completion_handler));
}

template <bool IsIntrusivelyListable, class Signature, class CompletionHandler>
auto allocate_local_operation(agrpc::GrpcContext& grpc_context, CompletionHandler&& completion_handler)

{
    auto allocator = detail::exec::get_allocator(completion_handler);
    if constexpr (detail::IS_STD_ALLOCATOR<decltype(allocator)>)
    {
        using Operation =
            detail::LocalOperation<IsIntrusivelyListable, detail::RemoveCrefT<CompletionHandler>, Signature>;
        return detail::allocate<Operation>(grpc_context.get_allocator(),
                                           std::forward<CompletionHandler>(completion_handler));
    }
    else
    {
        return detail::allocate_operation<IsIntrusivelyListable, Signature>(
            std::forward<CompletionHandler>(completion_handler));
    }
}

template <bool IsIntrusivelyListable, class Signature, class CompletionHandler, class OnLocalOperation,
          class OnRemoteOperation>
void allocate_operation_and_invoke(agrpc::GrpcContext& grpc_context, bool is_running_in_this_thread,
                                   CompletionHandler&& completion_handler, OnLocalOperation& on_local_operation,
                                   OnRemoteOperation& on_remote_operation)
{
    grpc_context.work_started();
    detail::WorkFinishedOnExit on_exit{grpc_context};
    if (is_running_in_this_thread)
    {
        auto operation = detail::allocate_local_operation<IsIntrusivelyListable, Signature>(
            grpc_context, std::forward<CompletionHandler>(completion_handler));
        on_local_operation(grpc_context, operation.get());
        operation.release();
    }
    else
    {
        auto operation = detail::allocate_operation<IsIntrusivelyListable, Signature>(
            std::forward<CompletionHandler>(completion_handler));
        on_remote_operation(grpc_context, operation.get());
        operation.release();
    }
    on_exit.release();
}

struct GrpcContextAddLocalOperation
{
    void operator()(agrpc::GrpcContext& grpc_context, detail::TypeErasedNoArgOperation* operation) const noexcept
    {
        detail::GrpcContextImplementation::add_local_operation(grpc_context, operation);
    }
};

struct GrpcContextAddRemoteOperation
{
    void operator()(agrpc::GrpcContext& grpc_context, detail::TypeErasedNoArgOperation* operation) const noexcept
    {
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, operation);
    }
};

template <bool IsBlockingNever, class CompletionHandler>
void create_and_submit_no_arg_operation(agrpc::GrpcContext& grpc_context, CompletionHandler&& completion_handler)
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
            auto ch{std::forward<CompletionHandler>(completion_handler)};
            ch();
            return;
        }
    }
    detail::GrpcContextAddLocalOperation on_local_operation{};
    detail::GrpcContextAddRemoteOperation on_remote_operation{};
    detail::allocate_operation_and_invoke<true, void()>(grpc_context, is_running_in_this_thread,
                                                        std::forward<CompletionHandler>(completion_handler),
                                                        on_local_operation, on_remote_operation);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_OPERATION_HPP
