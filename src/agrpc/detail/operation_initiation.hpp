// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_DETAIL_OPERATION_INITIATION_HPP
#define AGRPC_DETAIL_OPERATION_INITIATION_HPP

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_handle.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <DeallocateOnComplete Deallocate, class Initiation, class Operation, class... T>
void initiate(Operation& operation, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
              detail::AllocationType alloc_type, T...)
{
    if constexpr (Deallocate == detail::DeallocateOnComplete::YES_)
    {
        if (AllocationType::LOCAL == alloc_type)
        {
            initiation.initiate(OperationHandle<Operation, AllocationType::LOCAL>{operation, grpc_context},
                                operation.implementation());
        }
        else
        {
            initiation.initiate(OperationHandle<Operation, AllocationType::CUSTOM>{operation, grpc_context},
                                operation.implementation());
        }
    }
    else
    {
        initiation.initiate(OperationHandle<Operation, AllocationType::NONE>{operation, grpc_context},
                            operation.implementation());
    }
}

template <DeallocateOnComplete, class Initiation, template <class, class> class Operation, class Implementation,
          class T>
auto initiate(Operation<Implementation, T>& operation, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
              detail::AllocationType)
    -> decltype((void)initiation.initiate(grpc_context, std::declval<Implementation&>(), nullptr))
{
    initiation.initiate(grpc_context, operation.implementation(), operation.tag());
}

template <DeallocateOnComplete, class Initiation, template <class, class> class Operation, class Implementation,
          class T>
auto initiate(Operation<Implementation, T>& operation, agrpc::GrpcContext& grpc_context, const Initiation& initiation,
              detail::AllocationType) -> decltype((void)initiation.initiate(grpc_context, nullptr))
{
    initiation.initiate(grpc_context, operation.tag());
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_INITIATION_HPP
