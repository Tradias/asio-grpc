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

#ifndef AGRPC_DETAIL_ONESHOTALLOCATOR_HPP
#define AGRPC_DETAIL_ONESHOTALLOCATOR_HPP

#include <agrpc/detail/config.hpp>

#include <cassert>
#include <cstddef>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
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

    explicit OneShotAllocator(void* buffer) noexcept : buffer(buffer) {}

    template <class U>
    OneShotAllocator(const detail::OneShotAllocator<U, Capacity>& other) noexcept : buffer(other.buffer)
    {
    }

    [[nodiscard]] T* allocate([[maybe_unused]] std::size_t n) noexcept
    {
        static_assert(Capacity >= sizeof(T), "OneShotAllocator has insufficient capacity");
        assert(Capacity >= n * sizeof(T));
        void* ptr = this->buffer;
        assert(std::exchange(this->buffer, nullptr));
        return static_cast<T*>(ptr);
    }

    static void deallocate(T*, std::size_t) noexcept {}

    template <class U, std::size_t OtherCapacity>
    friend bool operator==(const OneShotAllocator& lhs, const detail::OneShotAllocator<U, OtherCapacity>& rhs) noexcept
    {
        return lhs.buffer == rhs.buffer;
    }

    template <class U, std::size_t OtherCapacity>
    friend bool operator!=(const OneShotAllocator& lhs, const detail::OneShotAllocator<U, OtherCapacity>& rhs) noexcept
    {
        return lhs.buffer != rhs.buffer;
    }

  private:
    template <class, std::size_t>
    friend class detail::OneShotAllocator;

    void* buffer;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ONESHOTALLOCATOR_HPP
