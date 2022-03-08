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

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/utility.hpp"

#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Allocator>
void deallocate(Allocator allocator, typename std::allocator_traits<Allocator>::pointer ptr)
{
    using Traits = std::allocator_traits<Allocator>;
    Traits::destroy(allocator, ptr);
    Traits::deallocate(allocator, ptr, 1);
}

template <class Allocator>
class AllocatedPointer
{
  private:
    using Traits = std::allocator_traits<Allocator>;

  public:
    using allocator_type = Allocator;

    constexpr AllocatedPointer(typename Traits::pointer ptr, const allocator_type& allocator) noexcept
        : impl(ptr, allocator)
    {
    }

    AllocatedPointer(const AllocatedPointer&) = delete;
    AllocatedPointer& operator=(const AllocatedPointer&) = delete;

    constexpr AllocatedPointer(AllocatedPointer&& other) noexcept
        : impl(std::exchange(other.get(), nullptr), other.get_allocator())
    {
    }

    constexpr AllocatedPointer& operator=(AllocatedPointer&& other) noexcept
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
            detail::deallocate(this->get_allocator(), this->get());
        }
    }

    constexpr decltype(auto) get() noexcept { return this->impl.first(); }

    constexpr decltype(auto) get() const noexcept { return this->impl.first(); }

    constexpr decltype(auto) get_allocator() noexcept { return this->impl.second(); }

    constexpr decltype(auto) get_allocator() const noexcept { return this->impl.second(); }

    constexpr decltype(auto) operator->() noexcept { return this->get(); }

    constexpr decltype(auto) operator->() const noexcept { return this->get(); }

    constexpr decltype(auto) operator*() noexcept { return *this->get(); }

    constexpr decltype(auto) operator*() const noexcept { return *this->get(); }

    constexpr void release() noexcept { this->get() = nullptr; }

    constexpr void reset() noexcept
    {
        detail::deallocate(this->get_allocator(), this->get());
        this->release();
    }

  private:
    detail::CompressedPair<typename Traits::pointer, allocator_type> impl;
};

template <class T, class Allocator>
AllocatedPointer(T*, const Allocator&)
    -> AllocatedPointer<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;

template <class Allocator>
class AllocationGuard
{
  private:
    using Traits = std::allocator_traits<Allocator>;

  public:
    using allocator_type = Allocator;

    constexpr AllocationGuard(typename Traits::pointer ptr, allocator_type allocator) noexcept
        : ptr(ptr), allocator(std::move(allocator))
    {
    }

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

    constexpr detail::AllocatedPointer<Allocator> release() noexcept
    {
        this->is_allocated = false;
        return {this->ptr, this->allocator};
    }

    constexpr auto& get_allocator() noexcept { return allocator; }

  private:
    typename Traits::pointer ptr;
    allocator_type allocator;
    bool is_allocated{true};
};

template <class T, class Allocator, class... Args>
auto allocate(Allocator allocator, Args&&... args)
{
    using Traits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;
    using ReboundAllocator = typename Traits::allocator_type;
    ReboundAllocator rebound_allocator{allocator};
    auto* ptr = Traits::allocate(rebound_allocator, 1);
    detail::AllocationGuard<ReboundAllocator> guard{ptr, rebound_allocator};
    Traits::construct(guard.get_allocator(), ptr, std::forward<Args>(args)...);
    return guard.release();
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALLOCATE_HPP
