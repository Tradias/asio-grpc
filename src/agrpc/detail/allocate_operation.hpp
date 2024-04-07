// Copyright 2023 Dennis Hezel
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
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

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
    const auto allocator = detail::get_allocator(handler);
    auto operation = detail::allocate<OperationTemplate<detail::RemoveCrefT<Handler>>>(
        allocator, detail::AllocationType::CUSTOM, static_cast<Handler&&>(handler), static_cast<Args&&>(args)...);
    return operation.release();
}

template <template <class> class OperationTemplate, class Handler, class... Args>
auto allocate_local_operation(agrpc::GrpcContext& grpc_context, Handler&& handler, Args&&... args)
{
    using DecayedHandler = detail::RemoveCrefT<Handler>;
    auto allocator = detail::get_allocator(handler);
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
auto allocate_operation(bool is_running_in_this_thread, agrpc::GrpcContext& grpc_context, Handler&& handler,
                        Args&&... args)
{
    if (is_running_in_this_thread)
    {
        return detail::allocate_local_operation<OperationTemplate>(grpc_context, static_cast<Handler&&>(handler),
                                                                   static_cast<Args&&>(args)...);
    }
    return detail::allocate_custom_operation<OperationTemplate>(static_cast<Handler&&>(handler),
                                                                static_cast<Args&&>(args)...);
}

template <template <class> class OperationTemplate, class Handler, class... Args>
auto allocate_operation(agrpc::GrpcContext& grpc_context, Handler&& handler, Args&&... args)
{
    return detail::allocate_operation<OperationTemplate>(
        detail::GrpcContextImplementation::running_in_this_thread(grpc_context), grpc_context,
        static_cast<Handler&&>(handler), static_cast<Args&&>(args)...);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_OPERATION_HPP
