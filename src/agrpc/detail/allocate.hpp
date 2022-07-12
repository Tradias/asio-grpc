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
            AllocatedPointer::destroy_deallocate(this->get_allocator(), this->get());
        }
    }

    Pointer& get() noexcept { return this->impl.first(); }

    Pointer get() const noexcept { return this->impl.first(); }

    Allocator& get_allocator() noexcept { return this->impl.second(); }

    const Allocator& get_allocator() const noexcept { return this->impl.second(); }

    Pointer operator->() const noexcept { return this->get(); }

    Value& operator*() const noexcept { return *this->get(); }

    Pointer release() noexcept { return std::exchange(this->get(), nullptr); }

    void reset() noexcept
    {
        AllocatedPointer::destroy_deallocate(this->get_allocator(), this->get());
        this->release();
    }

  private:
    static void destroy_deallocate(Allocator& allocator, Pointer ptr) noexcept
    {
        Traits::destroy(allocator, ptr);
        Traits::deallocate(allocator, ptr, 1);
    }

    detail::CompressedPair<Pointer, Allocator> impl;
};

template <class T, class Allocator>
AllocatedPointer(T*, const Allocator&)
    -> AllocatedPointer<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;

template <class T, class Allocator>
using AllocatedPointerT = detail::AllocatedPointer<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;

template <class Allocator>
class AllocationGuard
{
  private:
    using Traits = std::allocator_traits<Allocator>;
    using Pointer = typename Traits::pointer;

  public:
    AllocationGuard(Pointer ptr, Allocator& allocator) noexcept : ptr(ptr), allocator(allocator) {}

    AllocationGuard(const AllocationGuard&) = delete;
    AllocationGuard(AllocationGuard&&) = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;
    AllocationGuard& operator=(AllocationGuard&&) = delete;

    ~AllocationGuard() noexcept
    {
        if (this->is_allocated)
        {
            Traits::deallocate(this->allocator, this->ptr, 1);
        }
    }

    detail::AllocatedPointer<Allocator> release() noexcept
    {
        this->is_allocated = false;
        return {this->ptr, this->allocator};
    }

    Allocator& get_allocator() noexcept { return allocator; }

  private:
    Pointer ptr;
    Allocator& allocator;
    bool is_allocated{true};
};

template <class T, class Allocator, class... Args>
detail::AllocatedPointerT<T, Allocator> allocate(const Allocator& allocator, Args&&... args)
{
    using Traits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    auto* ptr = Traits::allocate(rebound_allocator, 1);
    detail::AllocationGuard<ReboundAllocator> guard{ptr, rebound_allocator};
    Traits::construct(rebound_allocator, ptr, std::forward<Args>(args)...);
    return guard.release();
}

template <class T, class Allocator>
void destroy_deallocate(T* ptr, const Allocator& allocator) noexcept
{
    using Traits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    Traits::destroy(rebound_allocator, ptr);
    Traits::deallocate(rebound_allocator, ptr, 1);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_HPP
