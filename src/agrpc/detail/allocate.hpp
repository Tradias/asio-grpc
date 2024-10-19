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

template <class T>
inline constexpr bool IS_STD_ALLOCATOR = false;

template <class T>
inline constexpr bool IS_STD_ALLOCATOR<std::allocator<T>> = true;

template <class Traits>
class AllocationGuard
{
  private:
    using ValueType = typename Traits::value_type;
    using Allocator = typename Traits::allocator_type;
    using Pointer = typename Traits::pointer;

  public:
    AllocationGuard(Pointer&& ptr, const Allocator& allocator) noexcept
        : ptr_(static_cast<Pointer&&>(ptr)), allocator_(allocator)
    {
    }

    AllocationGuard(ValueType& value, const Allocator& allocator) noexcept
        : ptr_(std::pointer_traits<Pointer>::pointer_to(value)), allocator_(allocator)
    {
    }

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

    ValueType& operator*() const noexcept { return *ptr_; }

    void release() noexcept { ptr_ = nullptr; }

    [[nodiscard]] ValueType* extract() noexcept { return std::addressof(*std::exchange(ptr_, nullptr)); }

    void reset() noexcept
    {
        destroy_deallocate_using_traits();
        release();
    }

  private:
    void destroy_deallocate_using_traits() noexcept
    {
        Traits::destroy(allocator_, std::addressof(*ptr_));
        Traits::deallocate(allocator_, ptr_, 1);
    }

    Pointer ptr_;
    Allocator allocator_;
};

template <class T, class Allocator>
AllocationGuard(T&, const Allocator&) -> AllocationGuard<detail::RebindAllocatorTraits<T, Allocator>>;

template <class T, class Allocator, class... Args>
detail::AllocationGuard<detail::RebindAllocatorTraits<T, Allocator>> allocate(const Allocator& allocator,
                                                                              Args&&... args)
{
    using Traits = detail::RebindAllocatorTraits<T, Allocator>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    auto&& ptr = Traits::allocate(rebound_allocator, 1);
    detail::ScopeGuard guard{[&]
                             {
                                 Traits::deallocate(rebound_allocator, ptr, 1);
                             }};
    Traits::construct(rebound_allocator, std::addressof(*ptr), static_cast<Args&&>(args)...);
    guard.release();
    return {static_cast<decltype(ptr)&&>(ptr), rebound_allocator};
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_HPP
