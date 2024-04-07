// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_HELPER_ONE_SHOT_ALLOCATOR_HPP
#define AGRPC_HELPER_ONE_SHOT_ALLOCATOR_HPP

#include <cassert>
#include <cstddef>
#include <utility>

namespace example
{
template <class T, std::size_t Capacity>
class OneShotAllocator
{
  public:
    using value_type = T;

    template <class U>
    struct rebind
    {
        using other = OneShotAllocator<U, Capacity>;
    };

    OneShotAllocator() = default;

    explicit OneShotAllocator(void* buffer) noexcept : buffer(buffer) {}

    template <class U>
    OneShotAllocator(const OneShotAllocator<U, Capacity>& other) noexcept : buffer(other.buffer)
    {
    }

    [[nodiscard]] T* allocate([[maybe_unused]] std::size_t n) noexcept
    {
        static_assert(Capacity >= sizeof(T), "OneShotAllocator has insufficient capacity");
        static_assert(alignof(std::max_align_t) >= alignof(T), "Overaligned types are not supported");
        assert(Capacity >= n * sizeof(T));
        return static_cast<T*>(buffer);
    }

    static void deallocate(T*, std::size_t) noexcept {}

    template <class U, std::size_t OtherCapacity>
    friend bool operator==(const OneShotAllocator& lhs, const OneShotAllocator<U, OtherCapacity>& rhs) noexcept
    {
        return lhs.buffer == rhs.buffer;
    }

    template <class U, std::size_t OtherCapacity>
    friend bool operator!=(const OneShotAllocator& lhs, const OneShotAllocator<U, OtherCapacity>& rhs) noexcept
    {
        return lhs.buffer != rhs.buffer;
    }

  private:
    template <class, std::size_t>
    friend class OneShotAllocator;

    void* buffer;
};
}

#endif  // AGRPC_HELPER_ONE_SHOT_ALLOCATOR_HPP
