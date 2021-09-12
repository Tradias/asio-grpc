// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPCEXECUTOROPERATION_HPP
#define AGRPC_DETAIL_GRPCEXECUTOROPERATION_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/functional.hpp"
#include "agrpc/detail/grpcContextOperation.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/utility.hpp"

namespace agrpc::detail
{
template <bool IsIntrusivelyListable, class Handler, class Allocator>
class GrpcExecutorOperation : public detail::BasicGrpcContextOperation<IsIntrusivelyListable>
{
  private:
    using Base = detail::BasicGrpcContextOperation<IsIntrusivelyListable>;

  public:
    template <class H>
    GrpcExecutorOperation(H&& handler, Allocator allocator)
        : Base(&GrpcExecutorOperation::do_complete), impl(std::forward<H>(handler), std::move(allocator))
    {
    }

    static void do_complete(Base* base, bool ok, detail::InvokeHandler invoke_handler)
    {
        using ReboundAllocator =
            typename std::allocator_traits<Allocator>::template rebind_alloc<GrpcExecutorOperation>;
        auto* self = static_cast<GrpcExecutorOperation*>(base);
        detail::AllocatedPointer<ReboundAllocator> ptr{self, self->get_allocator()};
        if (invoke_handler == detail::InvokeHandler::YES)
        {
            // Make a copy of the handler so that the memory can be deallocated before the upcall is made.
            auto handler{std::move(self->handler())};
            ptr.reset();
            detail::invoke_front(handler, ok);
        }
    }

    [[nodiscard]] decltype(auto) handler() noexcept { return impl.first(); }

    [[nodiscard]] decltype(auto) handler() const noexcept { return impl.first(); }

    [[nodiscard]] decltype(auto) get_allocator() noexcept { return impl.second(); }

    [[nodiscard]] decltype(auto) get_allocator() const noexcept { return impl.second(); }

  private:
    detail::CompressedPair<Handler, Allocator> impl;
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCEXECUTOROPERATION_HPP
