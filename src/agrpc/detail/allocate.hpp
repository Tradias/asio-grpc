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
    AllocatedPointer(Pointer ptr, const Allocator& allocator) noexcept : impl_(ptr, allocator) {}

    AllocatedPointer(const AllocatedPointer&) = delete;
    AllocatedPointer& operator=(const AllocatedPointer&) = delete;

    AllocatedPointer(AllocatedPointer&& other) noexcept
        : impl_(std::exchange(other.get(), nullptr), other.get_allocator())
    {
    }

    template <class Alloc = Allocator, class = std::enable_if_t<std::allocator_traits<Alloc>::is_always_equal::value>>
    AllocatedPointer& operator=(AllocatedPointer&& other) noexcept
    {
        if (this != &other)
        {
            get() = std::exchange(other.get(), nullptr);
        }
        return *this;
    }

    ~AllocatedPointer() noexcept
    {
        if (get())
        {
            detail::destroy_deallocate_using_traits<Traits>(get_allocator(), get());
        }
    }

    Pointer& get() noexcept { return impl_.first(); }

    Pointer get() const noexcept { return impl_.first(); }

    Allocator& get_allocator() noexcept { return impl_.second(); }

    Pointer operator->() const noexcept { return get(); }

    Pointer release() noexcept { return std::exchange(get(), nullptr); }

    void reset() noexcept
    {
        detail::destroy_deallocate_using_traits<Traits>(get_allocator(), get());
        release();
    }

  private:
    detail::CompressedPair<Pointer, Allocator> impl_;
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
    AllocationGuard(Pointer ptr, const Allocator& allocator) noexcept : ptr_(ptr), allocator_(allocator) {}

    AllocationGuard(const AllocationGuard&) = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;
    AllocationGuard(AllocationGuard&&) = delete;
    AllocationGuard& operator=(AllocationGuard&&) = delete;

    ~AllocationGuard() noexcept
    {
        if (ptr_)
        {
            detail::destroy_deallocate_using_traits<Traits>(allocator_, ptr_);
        }
    }

    Pointer get() const noexcept { return ptr_; }

    Allocator get_allocator() const noexcept { return allocator_; }

    Pointer operator->() const noexcept { return ptr_; }

    auto& operator*() const noexcept { return *ptr_; }

    Pointer release() noexcept { return std::exchange(ptr_, nullptr); }

    void reset() noexcept
    {
        detail::destroy_deallocate_using_traits<Traits>(allocator_, ptr_);
        release();
    }

  private:
    Pointer ptr_;
    Allocator allocator_;
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
    UninitializedAllocationGuard(Pointer ptr, Allocator& allocator) noexcept : ptr_(ptr), allocator_(allocator) {}

    UninitializedAllocationGuard(const UninitializedAllocationGuard&) = delete;
    UninitializedAllocationGuard(UninitializedAllocationGuard&&) = delete;
    UninitializedAllocationGuard& operator=(const UninitializedAllocationGuard&) = delete;
    UninitializedAllocationGuard& operator=(UninitializedAllocationGuard&&) = delete;

    ~UninitializedAllocationGuard() noexcept
    {
        if (ptr_)
        {
            Traits::deallocate(allocator_, ptr_, 1);
        }
    }

    detail::AllocationGuard<Traits> release() noexcept { return {std::exchange(ptr_, nullptr), allocator_}; }

    Allocator& get_allocator() noexcept { return allocator_; }

  private:
    Pointer ptr_;
    Allocator& allocator_;
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
    Traits::construct(rebound_allocator, ptr, static_cast<Args&&>(args)...);
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
