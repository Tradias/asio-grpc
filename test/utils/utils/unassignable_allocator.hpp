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

#ifndef AGRPC_UTILS_UNASSIGNABLE_ALLOCATOR_HPP
#define AGRPC_UTILS_UNASSIGNABLE_ALLOCATOR_HPP

#include <cstddef>
#include <memory>

namespace test
{
template <class T>
class UnassignableAllocator
{
  public:
    using value_type = T;

    UnassignableAllocator() = default;

    constexpr explicit UnassignableAllocator(void* marker) noexcept : marker(marker) {}

    template <class U>
    constexpr UnassignableAllocator(const test::UnassignableAllocator<U>& other) noexcept : marker(other.marker)
    {
    }

    UnassignableAllocator(const UnassignableAllocator&) = default;

    UnassignableAllocator(UnassignableAllocator&&) = default;

    UnassignableAllocator& operator=(const UnassignableAllocator&) = delete;

    UnassignableAllocator& operator=(UnassignableAllocator&&) = delete;

    [[nodiscard]] static T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }

    static void deallocate(T* p, std::size_t n) { std::allocator<T>{}.deallocate(p, n); }

    template <class U>
    friend constexpr bool operator==(const UnassignableAllocator& lhs,
                                     const test::UnassignableAllocator<U>& rhs) noexcept
    {
        return lhs.marker == rhs.marker;
    }

    template <class U>
    friend constexpr bool operator!=(const UnassignableAllocator& lhs,
                                     const test::UnassignableAllocator<U>& rhs) noexcept
    {
        return lhs.marker != rhs.marker;
    }

  private:
    template <class>
    friend class test::UnassignableAllocator;

    void* marker;
};
}

#endif  // AGRPC_UTILS_UNASSIGNABLE_ALLOCATOR_HPP
