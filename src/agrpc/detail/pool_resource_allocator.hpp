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

    PoolResourceAllocator(const PoolResourceAllocator&) = default;

    template <class U>
    PoolResourceAllocator(const detail::PoolResourceAllocator<U>&) noexcept
    {
    }

    PoolResourceAllocator& operator=(const PoolResourceAllocator& other) = default;

    PoolResourceAllocator& operator=(PoolResourceAllocator&& other) = default;

    [[nodiscard]] static T* allocate(std::size_t n);

    static void deallocate(T* p, std::size_t n) noexcept;

    template <class U>
    friend bool operator==(const PoolResourceAllocator&, const detail::PoolResourceAllocator<U>&) noexcept
    {
        return true;
    }

    template <class U>
    friend bool operator!=(const PoolResourceAllocator&, const detail::PoolResourceAllocator<U>&) noexcept
    {
        return false;
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POOL_RESOURCE_ALLOCATOR_HPP
