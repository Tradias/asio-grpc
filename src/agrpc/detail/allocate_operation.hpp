// Copyright 2024 Dennis Hezel
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
auto allocate_operation(Handler&& handler, Args&&... args)
{
    using DecayedHandler = detail::RemoveCrefT<Handler>;
    using Op = OperationTemplate<DecayedHandler>;
    using Allocator = detail::AssociatedAllocatorT<DecayedHandler>;
    if constexpr (detail::IS_STD_ALLOCATOR<Allocator>)
    {
        if (GrpcContextImplementation::running_in_this_thread())
        {
            return detail::allocate<Op>(detail::get_local_allocator(), detail::AllocationType::LOCAL,
                                        static_cast<Handler&&>(handler), static_cast<Args&&>(args)...)
                .extract();
        }
        return detail::allocate<Op>(std::allocator<Op>{}, detail::AllocationType::CUSTOM,
                                    static_cast<Handler&&>(handler), static_cast<Args&&>(args)...)
            .extract();
    }
    else
    {
        const auto allocator = detail::get_allocator(handler);
        return detail::allocate<Op>(allocator, detail::AllocationType::CUSTOM, static_cast<Handler&&>(handler),
                                    static_cast<Args&&>(args)...)
            .extract();
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_OPERATION_HPP
