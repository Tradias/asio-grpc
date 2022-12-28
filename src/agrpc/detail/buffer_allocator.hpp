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

#ifndef AGRPC_DETAIL_BUFFER_ALLOCATOR_HPP
#define AGRPC_DETAIL_BUFFER_ALLOCATOR_HPP

#include <agrpc/detail/config.hpp>

#include <cstddef>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T, class Buffer>
class BufferAllocator
{
  public:
    using value_type = T;

    BufferAllocator() = default;

    explicit BufferAllocator(Buffer& buffer) noexcept : buffer_(&buffer) {}

    template <class U>
    BufferAllocator(const detail::BufferAllocator<U, Buffer>& other) noexcept : buffer_(other.buffer_)
    {
    }

    [[nodiscard]] T* allocate(std::size_t n) noexcept
    {
        static_assert(alignof(std::max_align_t) >= alignof(T), "Overaligned types are not supported");
        static_assert(Buffer::max_size() >= sizeof(T), "Insufficient buffer size");
        return static_cast<T*>(buffer_->allocate(n * sizeof(T)));
    }

    static void deallocate(T*, std::size_t) noexcept {}

    template <class U>
    friend bool operator==(const BufferAllocator& lhs, const detail::BufferAllocator<U, Buffer>& rhs) noexcept
    {
        return lhs.buffer_ == rhs.buffer;
    }

    template <class U>
    friend bool operator!=(const BufferAllocator& lhs, const detail::BufferAllocator<U, Buffer>& rhs) noexcept
    {
        return lhs.buffer_ != rhs.buffer;
    }

  private:
    template <class, class>
    friend class detail::BufferAllocator;

    Buffer* buffer_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_BUFFER_ALLOCATOR_HPP
