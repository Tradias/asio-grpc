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

#ifndef AGRPC_DETAIL_POOL_RESOURCE_ALLOCATOR_HPP
#define AGRPC_DETAIL_POOL_RESOURCE_ALLOCATOR_HPP

#include <agrpc/detail/pool_resource.hpp>

#include <cstddef>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
class PoolResourceAllocator
{
  public:
    using value_type = T;

    PoolResourceAllocator() = default;

    explicit PoolResourceAllocator(PoolResource* resource) noexcept : resource_(resource) {}

    PoolResourceAllocator(const PoolResourceAllocator&) = default;

    template <class U>
    PoolResourceAllocator(const detail::PoolResourceAllocator<U>& other) noexcept : resource_(other.resource_)
    {
    }

    PoolResourceAllocator& operator=(const PoolResourceAllocator& other) = delete;

    PoolResourceAllocator& operator=(PoolResourceAllocator&& other) = delete;

    [[nodiscard]] T* allocate(std::size_t n)
    {
        static_assert(alignof(T) <= MAX_ALIGN, "Overaligned types are not supported");
        return static_cast<T*>(resource_->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) noexcept { resource_->deallocate(p, n * sizeof(T), alignof(T)); }

    template <class U>
    friend bool operator==(const PoolResourceAllocator& lhs, const detail::PoolResourceAllocator<U>& rhs) noexcept
    {
        return lhs.resource_ == rhs.resource_;
    }

    template <class U>
    friend bool operator!=(const PoolResourceAllocator& lhs, const detail::PoolResourceAllocator<U>& rhs) noexcept
    {
        return lhs.resource_ != rhs.resource_;
    }

  private:
    template <class>
    friend class detail::PoolResourceAllocator;

    PoolResource* resource_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POOL_RESOURCE_ALLOCATOR_HPP
