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
template <AllocationType AllocType, int Id, template <class, class> class Operation, class Implementation, class T,
          class... Args>
auto complete_impl(Operation<Implementation, T>& operation, agrpc::GrpcContext& grpc_context, Args&&... args)
    -> decltype((void)std::declval<Implementation&>().done(
        std::declval<OperationHandle<Operation<Implementation, T>, AllocType, Id>>(), static_cast<Args&&>(args)...))
{
    operation.implementation().done(
        OperationHandle<Operation<Implementation, T>, AllocType, Id>{operation, grpc_context},
        static_cast<Args&&>(args)...);
}

template <AllocationType AllocType, int Id, template <class, class> class Operation, class Implementation, class T,
          class... Args>
auto complete_impl(Operation<Implementation, T>& operation, agrpc::GrpcContext& grpc_context, Args&&... args)
    -> decltype((void)std::declval<Implementation&>().done(grpc_context, static_cast<Args&&>(args)...))
{
    operation.implementation().done(grpc_context, args...);
    operation.template complete<AllocType>(grpc_context, static_cast<Args&&>(args)...);
}

template <AllocationType AllocType, int Id, template <class, class> class Operation, class Implementation, class T>
void complete(Operation<Implementation, T>& operation, detail::OperationResult result, agrpc::GrpcContext& grpc_context)
{
    if constexpr (Implementation::TYPE == detail::SenderImplementationType::BOTH ||
                  Implementation::TYPE == detail::SenderImplementationType::GRPC_TAG)
    {
        detail::complete_impl<AllocType, Id>(operation, grpc_context, detail::is_ok(result));
    }
    else
    {
        detail::complete_impl<AllocType, Id>(operation, grpc_context);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_IMPLEMENTATION_HPP
