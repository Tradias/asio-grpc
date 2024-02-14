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

#ifndef AGRPC_DETAIL_OPERATION_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_OPERATION_IMPLEMENTATION_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/detail/operation_handle.hpp>
#include <agrpc/detail/sender_implementation.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <AllocationType AllocType, int Id, class Operation>
void complete(Operation& operation, [[maybe_unused]] detail::OperationResult result, agrpc::GrpcContext& grpc_context)
{
    using Implementation = typename Operation::Implementation;
    const auto impl = [&](auto&&... args)
    {
        if constexpr (Implementation::NEEDS_ON_COMPLETE)
        {
            operation.implementation().complete(
                detail::OperationHandle<Operation, AllocType, Id>{operation, grpc_context},
                static_cast<decltype(args)&&>(args)...);
        }
        else
        {
            operation.implementation().complete(grpc_context, args...);
            operation.template complete<AllocType>(grpc_context, static_cast<decltype(args)&&>(args)...);
        }
    };
    if constexpr (Implementation::TYPE == detail::SenderImplementationType::BOTH ||
                  Implementation::TYPE == detail::SenderImplementationType::GRPC_TAG)
    {
        impl(detail::is_ok(result));
    }
    else
    {
        impl();
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_IMPLEMENTATION_HPP
