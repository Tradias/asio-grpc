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

#ifndef AGRPC_UTILS_COUNTINGALLOCATOR_HPP
#define AGRPC_UTILS_COUNTINGALLOCATOR_HPP

#include <cstddef>
#include <memory>

namespace test
{
template <class T>
class CountingAllocator
{
  public:
    using value_type = T;

    CountingAllocator() = default;

    constexpr explicit CountingAllocator(std::size_t& counter) noexcept : counter(&counter) {}

    template <class U>
    constexpr CountingAllocator(const test::CountingAllocator<U>& other) noexcept : counter(other.counter)
    {
    }

    [[nodiscard]] T* allocate(std::size_t n)
    {
        *this->counter += n * sizeof(T);
        return std::allocator<T>{}.allocate(n);
    }

    static void deallocate(T* p, std::size_t n) { std::allocator<T>{}.deallocate(p, n); }

    template <class U>
    friend constexpr bool operator==(const CountingAllocator& lhs, const test::CountingAllocator<U>& rhs) noexcept
    {
        return lhs.counter == rhs.counter;
    }

    template <class U>
    friend constexpr bool operator!=(const CountingAllocator& lhs, const test::CountingAllocator<U>& rhs) noexcept
    {
        return lhs.counter != rhs.counter;
    }

  private:
    template <class>
    friend class CountingAllocator;

    std::size_t* counter;
};
}

#endif  // AGRPC_UTILS_COUNTINGALLOCATOR_HPP
