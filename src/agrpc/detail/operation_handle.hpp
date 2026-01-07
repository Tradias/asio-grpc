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

#ifndef AGRPC_DETAIL_OPERATION_HANDLE_HPP
#define AGRPC_DETAIL_OPERATION_HANDLE_HPP

#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/deallocate_on_complete.hpp>
#include <agrpc/detail/forward.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Operation, detail::AllocationType AllocType>
struct BasicOperationHandle
{
    template <int Id = 0>
    class Type
    {
      public:
        Type(Operation& self, agrpc::GrpcContext& grpc_context) noexcept : self_(self), grpc_context_(grpc_context) {}

        template <class... Args>
        void operator()(Args&&... args)
        {
            self_.template complete<AllocType>(static_cast<Args&&>(args)...);
        }

        void done() { self_.done(); }

        template <int NextId = Id>
        [[nodiscard]] auto* tag() const noexcept
        {
            if constexpr (NextId != Id)
            {
                self_.template set_on_complete<AllocType, NextId>();
            }
            return self_.tag();
        }

        [[nodiscard]] agrpc::GrpcContext& grpc_context() const noexcept { return grpc_context_; }

      private:
        Operation& self_;
        agrpc::GrpcContext& grpc_context_;
    };
};

template <class Operation, detail::AllocationType AllocType, int Id = 0>
using OperationHandle = typename BasicOperationHandle<Operation, AllocType>::template Type<Id>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_HANDLE_HPP
