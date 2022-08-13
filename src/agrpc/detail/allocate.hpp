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

#ifndef AGRPC_DETAIL_ALLOCATE_HPP
#define AGRPC_DETAIL_ALLOCATE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T, class Allocator>
using RebindAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

template <class T, class Allocator>
using RebindAllocatorTraits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;

template <class Traits>
void destroy_deallocate_using_traits(typename Traits::allocator_type& allocator, typename Traits::pointer ptr) noexcept
{
    Traits::destroy(allocator, ptr);
    Traits::deallocate(allocator, ptr, 1);
}

template <class Allocator>
class AllocatedPointer
{
  private:
    using Traits = std::allocator_traits<Allocator>;
    using Pointer = typename Traits::pointer;
    using Value = typename Traits::value_type;

  public:
    AllocatedPointer(Pointer ptr, const Allocator& allocator) noexcept : impl(ptr, allocator) {}

    AllocatedPointer(const AllocatedPointer&) = delete;
    AllocatedPointer& operator=(const AllocatedPointer&) = delete;

    AllocatedPointer(AllocatedPointer&& other) noexcept
        : impl(std::exchange(other.get(), nullptr), other.get_allocator())
    {
    }

    template <class Alloc = Allocator, class = std::enable_if_t<std::allocator_traits<Alloc>::is_always_equal::value>>
    AllocatedPointer& operator=(AllocatedPointer&& other) noexcept
    {
        if (this != &other)
        {
            this->get() = std::exchange(other.get(), nullptr);
        }
        return *this;
    }

    ~AllocatedPointer() noexcept
    {
        if (this->get())
        {
            detail::destroy_deallocate_using_traits<Traits>(this->get_allocator(), this->get());
        }
    }

    Pointer& get() noexcept { return this->impl.first(); }

    Pointer get() const noexcept { return this->impl.first(); }

    Allocator& get_allocator() noexcept { return this->impl.second(); }

    Pointer operator->() const noexcept { return this->get(); }

    Pointer release() noexcept { return std::exchange(this->get(), nullptr); }

    void reset() noexcept
    {
        detail::destroy_deallocate_using_traits<Traits>(this->get_allocator(), this->get());
        this->release();
    }

  private:
    detail::CompressedPair<Pointer, Allocator> impl;
};

template <class T, class Allocator>
AllocatedPointer(T*, const Allocator&) -> AllocatedPointer<detail::RebindAllocator<T, Allocator>>;

template <class Traits>
class AllocationGuard
{
  private:
    using Allocator = typename Traits::allocator_type;
    using Pointer = typename Traits::pointer;

  public:
    AllocationGuard(Pointer ptr, const Allocator& allocator) noexcept : ptr(ptr), allocator(allocator) {}

    AllocationGuard(const AllocationGuard&) = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;
    AllocationGuard(AllocationGuard&&) = delete;
    AllocationGuard& operator=(AllocationGuard&&) = delete;

    ~AllocationGuard() noexcept
    {
        if (ptr)
        {
            detail::destroy_deallocate_using_traits<Traits>(allocator, ptr);
        }
    }

    Pointer get() const noexcept { return ptr; }

    Allocator get_allocator() const noexcept { return allocator; }

    Pointer operator->() const noexcept { return ptr; }

    auto& operator*() const noexcept { return *ptr; }

    Pointer release() noexcept { return std::exchange(ptr, nullptr); }

    void reset() noexcept
    {
        detail::destroy_deallocate_using_traits<Traits>(allocator, ptr);
        this->release();
    }

  private:
    Pointer ptr;
    Allocator allocator;
};

template <class T, class Allocator>
AllocationGuard(T*, const Allocator&) -> AllocationGuard<detail::RebindAllocatorTraits<T, Allocator>>;

template <class Traits>
class UninitializedAllocationGuard
{
  private:
    using Allocator = typename Traits::allocator_type;
    using Pointer = typename Traits::pointer;

  public:
    UninitializedAllocationGuard(Pointer ptr, Allocator& allocator) noexcept : ptr(ptr), allocator(allocator) {}

    UninitializedAllocationGuard(const UninitializedAllocationGuard&) = delete;
    UninitializedAllocationGuard(UninitializedAllocationGuard&&) = delete;
    UninitializedAllocationGuard& operator=(const UninitializedAllocationGuard&) = delete;
    UninitializedAllocationGuard& operator=(UninitializedAllocationGuard&&) = delete;

    ~UninitializedAllocationGuard() noexcept
    {
        if (this->ptr)
        {
            Traits::deallocate(this->allocator, this->ptr, 1);
        }
    }

    detail::AllocationGuard<Traits> release() noexcept { return {std::exchange(this->ptr, nullptr), this->allocator}; }

    Allocator& get_allocator() noexcept { return allocator; }

  private:
    Pointer ptr;
    Allocator& allocator;
};

template <class T, class Allocator, class... Args>
detail::AllocationGuard<detail::RebindAllocatorTraits<T, Allocator>> allocate(const Allocator& allocator,
                                                                              Args&&... args)
{
    using Traits = detail::RebindAllocatorTraits<T, Allocator>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    auto* ptr = Traits::allocate(rebound_allocator, 1);
    detail::UninitializedAllocationGuard<Traits> guard{ptr, rebound_allocator};
    Traits::construct(rebound_allocator, ptr, std::forward<Args>(args)...);
    return guard.release();
}

template <class T, class Allocator>
void destroy_deallocate(T* ptr, const Allocator& allocator) noexcept
{
    using Traits = detail::RebindAllocatorTraits<T, Allocator>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    Traits::destroy(rebound_allocator, ptr);
    Traits::deallocate(rebound_allocator, ptr, 1);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_HPP
