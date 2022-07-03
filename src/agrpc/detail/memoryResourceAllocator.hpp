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

#ifndef AGRPC_DETAIL_MEMORYRESOURCEALLOCATOR_HPP
#define AGRPC_DETAIL_MEMORYRESOURCEALLOCATOR_HPP

#include <agrpc/detail/config.hpp>

#include <cstddef>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
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
    friend class detail::MemoryResourceAllocator;

    Resource* resource;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORYRESOURCEALLOCATOR_HPP
