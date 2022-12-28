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
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
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

template <template <class> class OperationTemplate, class Handler, class... Args>
auto allocate_custom_operation(Handler&& handler, Args&&... args)

{
    const auto allocator = detail::exec::get_allocator(handler);
    auto operation = detail::allocate<OperationTemplate<detail::RemoveCrefT<Handler>>>(
        allocator, detail::AllocationType::CUSTOM, static_cast<Handler&&>(handler), static_cast<Args&&>(args)...);
    return operation.release();
}

template <template <class> class OperationTemplate, class Handler, class... Args>
auto allocate_local_operation(agrpc::GrpcContext& grpc_context, Handler&& handler, Args&&... args)

{
    using DecayedHandler = detail::RemoveCrefT<Handler>;
    auto allocator = detail::exec::get_allocator(handler);
    if constexpr (detail::IS_STD_ALLOCATOR<decltype(allocator)>)
    {
        auto operation = detail::allocate<OperationTemplate<DecayedHandler>>(
            grpc_context.get_allocator(), detail::AllocationType::LOCAL, static_cast<Handler&&>(handler),
            static_cast<Args&&>(args)...);
        return operation.release();
    }
    else
    {
        auto operation = detail::allocate<OperationTemplate<DecayedHandler>>(
            allocator, detail::AllocationType::CUSTOM, static_cast<Handler&&>(handler), static_cast<Args&&>(args)...);
        return operation.release();
    }
}

template <template <class> class OperationTemplate, class Handler, class... Args>
auto allocate_operation(agrpc::GrpcContext& grpc_context, Handler&& handler, Args&&... args)
{
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        return detail::allocate_local_operation<OperationTemplate>(grpc_context, static_cast<Handler&&>(handler),
                                                                   static_cast<Args&&>(args)...);
    }
    return detail::allocate_custom_operation<OperationTemplate>(static_cast<Handler&&>(handler),
                                                                static_cast<Args&&>(args)...);
}

template <bool IsBlockingNever, class Handler>
void create_and_submit_no_arg_operation(agrpc::GrpcContext& grpc_context, Handler&& handler)
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
            auto op{static_cast<Handler&&>(handler)};
            std::move(op)();
            return;
        }
    }
    detail::StartWorkAndGuard guard{grpc_context};
    if (is_running_in_this_thread)
    {
        auto operation =
            detail::allocate_local_operation<detail::NoArgOperation>(grpc_context, static_cast<Handler&&>(handler));
        detail::GrpcContextImplementation::add_local_operation(grpc_context, operation);
    }
    else
    {
        auto operation = detail::allocate_custom_operation<detail::NoArgOperation>(static_cast<Handler&&>(handler));
        detail::GrpcContextImplementation::add_remote_operation(grpc_context, operation);
    }
    guard.release();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_OPERATION_HPP
