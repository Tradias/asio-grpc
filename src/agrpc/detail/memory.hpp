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

#ifndef AGRPC_DETAIL_MEMORY_HPP
#define AGRPC_DETAIL_MEMORY_HPP

#include <boost/type_traits/remove_cv_ref.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>

namespace agrpc::detail
{
template <class Allocator>
struct AllocationGuard
{
    using Traits = std::allocator_traits<Allocator>;
    using allocator_type = Allocator;

    typename Traits::pointer ptr;
    bool is_constructed;
    allocator_type allocator;

    AllocationGuard(typename Traits::pointer ptr, bool is_constructed, const allocator_type& allocator) noexcept
        : ptr(ptr), is_constructed(is_constructed), allocator(allocator)
    {
    }

    constexpr AllocationGuard(AllocationGuard&& other) noexcept
        : ptr(std::exchange(other.ptr, nullptr)), is_constructed(other.is_constructed), allocator(other.allocator)
    {
    }

    constexpr AllocationGuard& operator=(AllocationGuard&& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->ptr = std::exchange(other.ptr, nullptr);
        }
        return *this;
    }

    ~AllocationGuard() noexcept { this->destroy(); }

    auto get() const noexcept { return this->ptr; }

    void release() noexcept { this->ptr = nullptr; }

    void reset() noexcept
    {
        this->destroy();
        this->release();
    }

    void destroy() noexcept
    {
        if (this->ptr)
        {
            if (this->is_constructed)
            {
                Traits::destroy(allocator, ptr);
            }
            Traits::deallocate(allocator, ptr, 1);
        }
    }
};

template <class T, class Allocator, class... Args>
auto allocate_unique(const Allocator& allocator, Args&&... args)
{
    using Traits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;
    typename Traits::allocator_type rebound_allocator{allocator};
    auto* ptr = Traits::allocate(rebound_allocator, 1);
    AllocationGuard guard{ptr, false, rebound_allocator};
    Traits::construct(rebound_allocator, ptr, std::forward<Args>(args)...);
    guard.is_constructed = true;
    return guard;
}

template <class T, class Resource>
struct MemoryResourceAllocator
{
    using value_type = T;

    Resource* resource;

    constexpr explicit MemoryResourceAllocator(Resource* resource) noexcept : resource(resource) {}

    template <class U>
    constexpr MemoryResourceAllocator(const MemoryResourceAllocator<U, Resource>& other) noexcept
        : resource(other.resource)
    {
    }

    [[nodiscard]] T* allocate(std::size_t n)
    {
        return static_cast<T*>(this->resource->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept { this->resource->deallocate(p, n * sizeof(T), alignof(T)); }
};

template <class T, class U, class Resource>
constexpr bool operator==(const detail::MemoryResourceAllocator<T, Resource>& lhs,
                          const detail::MemoryResourceAllocator<U, Resource>& rhs) noexcept
{
    return lhs.resource == rhs.resource;
}

template <class T, class U, class Resource>
constexpr bool operator!=(const detail::MemoryResourceAllocator<T, Resource>& lhs,
                          const detail::MemoryResourceAllocator<U, Resource>& rhs) noexcept
{
    return lhs.resource != rhs.resource;
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_MEMORY_HPP
