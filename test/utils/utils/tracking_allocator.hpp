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

#ifndef AGRPC_UTILS_TRACKING_ALLOCATOR_HPP
#define AGRPC_UTILS_TRACKING_ALLOCATOR_HPP

#include <cstddef>
#include <memory>

namespace test
{
struct TrackedAllocation
{
    std::size_t bytes_allocated;
    std::size_t bytes_deallocated;
};

template <class T>
class TrackingAllocator
{
  public:
    using value_type = T;

    TrackingAllocator() = default;

    constexpr explicit TrackingAllocator(TrackedAllocation& tracked) noexcept : tracked(&tracked) {}

    template <class U>
    constexpr TrackingAllocator(const test::TrackingAllocator<U>& other) noexcept : tracked(other.tracked)
    {
    }

    [[nodiscard]] T* allocate(std::size_t n)
    {
        tracked->bytes_allocated += n * sizeof(T);
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n)
    {
        tracked->bytes_deallocated += n * sizeof(T);
        std::allocator<T>{}.deallocate(p, n);
    }

    template <class U>
    friend constexpr bool operator==(const TrackingAllocator& lhs, const test::TrackingAllocator<U>& rhs) noexcept
    {
        return lhs.tracked == rhs.tracked;
    }

    template <class U>
    friend constexpr bool operator!=(const TrackingAllocator& lhs, const test::TrackingAllocator<U>& rhs) noexcept
    {
        return lhs.tracked != rhs.tracked;
    }

  private:
    template <class>
    friend class test::TrackingAllocator;

    TrackedAllocation* tracked;
};
}

#endif  // AGRPC_UTILS_TRACKING_ALLOCATOR_HPP
