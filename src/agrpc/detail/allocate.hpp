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

#ifndef AGRPC_DETAIL_ALLOCATE_HPP
#define AGRPC_DETAIL_ALLOCATE_HPP

#include <agrpc/detail/utility.hpp>

#include <memory>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T, class Allocator>
using RebindAllocatorTraits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;

template <class Traits>
class AllocationGuard
{
  private:
    using Allocator = typename Traits::allocator_type;
    using Pointer = typename Traits::pointer;

  public:
    AllocationGuard(Pointer ptr, const Allocator& allocator) noexcept : ptr_(ptr), allocator_(allocator) {}

    AllocationGuard(AllocationGuard&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)), allocator_(other.allocator_)
    {
    }

    AllocationGuard& operator=(const AllocationGuard&) = delete;
    AllocationGuard& operator=(AllocationGuard&&) = delete;

    ~AllocationGuard() noexcept
    {
        if (ptr_)
        {
            destroy_deallocate_using_traits();
        }
    }

    Pointer get() const noexcept { return ptr_; }

    Allocator get_allocator() const noexcept { return allocator_; }

    Pointer operator->() const noexcept { return ptr_; }

    auto& operator*() const noexcept { return *ptr_; }

    Pointer release() noexcept { return std::exchange(ptr_, nullptr); }

    void reset() noexcept
    {
        destroy_deallocate_using_traits();
        release();
    }

  private:
    void destroy_deallocate_using_traits() noexcept
    {
        Traits::destroy(allocator_, ptr_);
        Traits::deallocate(allocator_, ptr_, 1);
    }

    Pointer ptr_;
    Allocator allocator_;
};

template <class T, class Allocator>
AllocationGuard(T*, const Allocator&) -> AllocationGuard<detail::RebindAllocatorTraits<T, Allocator>>;

template <class T, class Allocator, class... Args>
detail::AllocationGuard<detail::RebindAllocatorTraits<T, Allocator>> allocate(const Allocator& allocator,
                                                                              Args&&... args)
{
    using Traits = detail::RebindAllocatorTraits<T, Allocator>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    auto* ptr = Traits::allocate(rebound_allocator, 1);
    detail::ScopeGuard guard{[&]
                             {
                                 Traits::deallocate(rebound_allocator, ptr, 1);
                             }};
    Traits::construct(rebound_allocator, ptr, static_cast<Args&&>(args)...);
    guard.release();
    return {ptr, rebound_allocator};
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_HPP
