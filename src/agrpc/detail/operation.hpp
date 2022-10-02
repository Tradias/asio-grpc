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

#ifndef AGRPC_DETAIL_OPERATION_HPP
#define AGRPC_DETAIL_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/type_erased_operation.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <bool IsIntrusivelyListable, class Handler, class Signature>
class Operation;

template <bool IsIntrusivelyListable, class Handler, class... Signature>
class Operation<IsIntrusivelyListable, Handler, void(Signature...)>
    : public detail::TypeErasedOperation<IsIntrusivelyListable, Signature..., detail::GrpcContextLocalAllocator>
{
  private:
    using Base = detail::TypeErasedOperation<IsIntrusivelyListable, Signature..., detail::GrpcContextLocalAllocator>;

  public:
    template <class... Args>
    explicit Operation(detail::AllocationType allocation_type, Args&&... args)
        : Base(detail::AllocationType::LOCAL == allocation_type
                   ? &do_local_complete
                   : &detail::default_do_complete<Operation, Base, Signature...>),
          handler(static_cast<Args&&>(args)...)
    {
    }

    static void do_local_complete(Base* op, detail::InvokeHandler invoke_handler, Signature... args,
                                  detail::GrpcContextLocalAllocator allocator)
    {
        auto* self = static_cast<Operation*>(op);
        detail::AllocationGuard ptr{self, allocator};
        if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
        {
            auto local_handler{static_cast<Handler&&>(self->handler)};
            ptr.reset();
            static_cast<Handler&&>(local_handler)(static_cast<Signature&&>(args)...);
        }
    }

    [[nodiscard]] Handler& completion_handler() noexcept { return handler; }

    [[nodiscard]] auto get_allocator() noexcept { return detail::exec::get_allocator(handler); }

  private:
    Handler handler;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_HPP
