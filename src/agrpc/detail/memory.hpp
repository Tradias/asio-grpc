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

#ifndef AGRPC_DETAIL_MEMORY_HPP
#define AGRPC_DETAIL_MEMORY_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/utility.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
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
            this->destroy();
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
        this->destroy();
        this->release();
    }

  private:
    constexpr void destroy() noexcept
    {
        Traits::destroy(this->get_allocator(), this->get());
        Traits::deallocate(this->get_allocator(), this->get(), 1);
    }

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

template <class T, class Resource>
class MemoryResourceAllocator
{
  public:
    using value_type = T;

    MemoryResourceAllocator() = default;

    constexpr explicit MemoryResourceAllocator(Resource* resource) noexcept : resource(resource) {}

    template <class U>
    constexpr MemoryResourceAllocator(const detail::MemoryResourceAllocator<U, Resource>& other) noexcept
        : resource(other.resource)
    {
    }

    [[nodiscard]] T* allocate(std::size_t n)
    {
        return static_cast<T*>(this->resource->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept { this->resource->deallocate(p, n * sizeof(T), alignof(T)); }

    template <class U>
    friend constexpr bool operator==(const MemoryResourceAllocator& lhs,
                                     const detail::MemoryResourceAllocator<U, Resource>& rhs) noexcept
    {
        return lhs.resource == rhs.resource;
    }

    template <class U>
    friend constexpr bool operator!=(const MemoryResourceAllocator& lhs,
                                     const detail::MemoryResourceAllocator<U, Resource>& rhs) noexcept
    {
        return lhs.resource != rhs.resource;
    }

  private:
    template <class, class>
    friend class MemoryResourceAllocator;

    Resource* resource;
};

template <class T, std::size_t Capacity>
class OneShotAllocator
{
  public:
    using value_type = T;

    template <class U>
    struct rebind
    {
        using other = detail::OneShotAllocator<U, Capacity>;
    };

    OneShotAllocator() = default;

    constexpr explicit OneShotAllocator(void* buffer) noexcept : buffer(buffer) {}

    template <class U>
    constexpr OneShotAllocator(const detail::OneShotAllocator<U, Capacity>& other) noexcept : buffer(other.buffer)
    {
    }

    [[nodiscard]] constexpr T* allocate([[maybe_unused]] std::size_t n) noexcept
    {
        static_assert(Capacity >= sizeof(T), "OneShotAllocator has insufficient capacity");
        assert(Capacity >= n * sizeof(T));
        void* ptr = this->buffer;
        assert(std::exchange(this->buffer, nullptr));
        return static_cast<T*>(ptr);
    }

    static constexpr void deallocate(T*, std::size_t) noexcept {}

    template <class U, std::size_t OtherCapacity>
    friend constexpr bool operator==(const OneShotAllocator& lhs,
                                     const detail::OneShotAllocator<U, OtherCapacity>& rhs) noexcept
    {
        return lhs.buffer == rhs.buffer;
    }

    template <class U, std::size_t OtherCapacity>
    friend constexpr bool operator!=(const OneShotAllocator& lhs,
                                     const detail::OneShotAllocator<U, OtherCapacity>& rhs) noexcept
    {
        return lhs.buffer != rhs.buffer;
    }

  private:
    template <class, std::size_t>
    friend class OneShotAllocator;

    void* buffer;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_HPP
