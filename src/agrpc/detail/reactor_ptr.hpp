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

#ifndef AGRPC_DETAIL_REACTOR_PTR_HPP
#define AGRPC_DETAIL_REACTOR_PTR_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using ReactorDeallocateFn = void (*)(void*) noexcept;

struct ReactorAccess
{
    template <class Ptr, class Allocator, class... Args>
    static auto create(Allocator allocator, Args&&... args)
    {
        return Ptr{detail::allocate<ReactorPtrAllocation<typename Ptr::Allocation, Allocator>>(
                       allocator, allocator, static_cast<Args&&>(args)...)
                       .extract()
                       ->get()};
    }

    template <class Executor, class Arg>
    static void set_executor(detail::ReactorExecutorBase<Executor>& base, Arg&& arg)
    {
        ::new (static_cast<void*>(&base.executor_)) Executor(static_cast<Arg&&>(arg));
    }

    template <class Arg>
    static void set_executor(detail::ReactorExecutorBase<void>&, Arg&&)
    {
    }

    template <class Executor>
    static void destroy_executor(detail::ReactorExecutorBase<Executor>& base)
    {
        base.executor_.~Executor();
    }

    static void destroy_executor(detail::ReactorExecutorBase<void>&) {}

    template <class Reactor, class Executor>
    static void initialize_reactor(Reactor& reactor, Executor&& executor, ReactorDeallocateFn deallocate)
    {
        ReactorAccess::set_executor(reactor, static_cast<Executor&&>(executor));
        reactor.set_deallocate_function(deallocate);
    }
};

template <class RefCountedReactor, class Allocator>
class ReactorPtrAllocation
{
  public:
    template <class Executor, class... Args>
    ReactorPtrAllocation(Allocator allocator, Executor&& executor, Args&&... args)
        : value_(detail::SecondThenVariadic{}, static_cast<Allocator&&>(allocator), static_cast<Args&&>(args)...)
    {
        ReactorAccess::initialize_reactor(value_.first(), static_cast<Executor&&>(executor), &deallocate);
    }

    auto* get() noexcept { return &value_.first(); }

  private:
    static void deallocate(void* self) noexcept
    {
        auto* const ptr = static_cast<ReactorPtrAllocation*>(self);
        ReactorAccess::destroy_executor(ptr->value_.first());
        [[maybe_unused]] detail::AllocationGuard g{*ptr, ptr->value_.second()};
    }

    detail::CompressedPair<RefCountedReactor, Allocator> value_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REACTOR_PTR_HPP
