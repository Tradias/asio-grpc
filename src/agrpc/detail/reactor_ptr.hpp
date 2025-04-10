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
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T, class Allocator>
class ReactorPtrAllocation
{
  public:
    template <class... Args>
    ReactorPtrAllocation(Allocator allocator, typename T::executor_type&& executor, Args&&... args)
        : value_(detail::SecondThenVariadic{}, static_cast<Allocator&&>(allocator),
                 typename T::InitArg{static_cast<typename T::executor_type&&>(executor), &deallocate},
                 static_cast<Args&&>(args)...)
    {
    }

    auto* get() noexcept
    {
        auto ptr = &value_.first();
        // TODO
        if ((void*)ptr != (void*)this)
        {
            std::abort();
        }
        return ptr;
    }

  private:
    static void deallocate(void* self) noexcept
    {
        auto* const ptr = static_cast<ReactorPtrAllocation*>(self);
        [[maybe_unused]] detail::AllocationGuard g{*ptr, ptr->value_.second()};
    }

    detail::CompressedPair<T, Allocator> value_;
};

struct ReactorPtrAccess
{
    template <class Ptr, class Allocator, class... Args>
    static auto create(Allocator allocator, Args&&... args)
    {
        return Ptr{detail::allocate<ReactorPtrAllocation<typename Ptr::Allocation, Allocator>>(
                       allocator, allocator, static_cast<Args&&>(args)...)
                       .extract()
                       ->get()};
    }
};

using ReactorDeallocateFn = void (*)(void*) noexcept;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REACTOR_PTR_HPP
