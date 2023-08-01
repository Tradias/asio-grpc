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

#ifndef AGRPC_DETAIL_MEMORY_HPP
#define AGRPC_DETAIL_MEMORY_HPP

#include <agrpc/detail/config.hpp>

#include <cstddef>
#include <limits>
#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
inline constexpr auto MAX_ALIGN = alignof(std::max_align_t);
inline constexpr std::size_t MAX_ALIGN_MINUS_ONE = MAX_ALIGN - 1u;

[[nodiscard]] constexpr auto align(std::size_t position, std::size_t alignment) noexcept
{
    return (position + alignment - 1u) & ~(alignment - 1u);
}

struct MaxAlignedData
{
    static constexpr auto count(std::size_t size) noexcept { return detail::align(size, MAX_ALIGN) / MAX_ALIGN; }

    alignas(std::max_align_t) std::byte data_[MAX_ALIGN];
};

struct MaxAlignAllocator
{
    static void* allocate(std::size_t size)
    {
        return std::allocator<MaxAlignedData>{}.allocate(MaxAlignedData::count(size));
    }

    static void* allocate_already_max_aligned(std::size_t size)
    {
        return std::allocator<MaxAlignedData>{}.allocate(size / MAX_ALIGN);
    }

    static void deallocate(void* p, std::size_t size)
    {
        std::allocator<MaxAlignedData>{}.deallocate(static_cast<MaxAlignedData*>(p), MaxAlignedData::count(size));
    }

    static void deallocate_already_max_aligned(void* p, std::size_t size)
    {
        std::allocator<MaxAlignedData>{}.deallocate(static_cast<MaxAlignedData*>(p), size / MAX_ALIGN);
    }
};

template <class T>
struct UnwrapUniquePtr
{
    using Type = T;
};

template <class T>
struct UnwrapUniquePtr<std::unique_ptr<T>>
{
    using Type = T;
};

template <class T>
struct UnwrapUniquePtr<const std::unique_ptr<T>>
{
    using Type = T;
};

template <class T>
using UnwrapUniquePtrT = typename detail::UnwrapUniquePtr<T>::Type;

template <class T>
auto& unwrap_unique_ptr(T& t) noexcept
{
    return t;
}

template <class T>
auto& unwrap_unique_ptr(std::unique_ptr<T>& t) noexcept
{
    return *t;
}

template <class T>
auto& unwrap_unique_ptr(const std::unique_ptr<T>& t) noexcept
{
    return *t;
}

template <std::size_t Size>
class StackBuffer
{
  public:
    [[nodiscard]] static constexpr std::size_t max_size() noexcept { return Size; }

    [[nodiscard]] void* allocate(std::size_t) noexcept { return buffer_; }

  private:
    alignas(std::max_align_t) std::byte buffer_[Size];
};

class DelayedBuffer
{
  public:
    [[nodiscard]] static constexpr std::size_t max_size() noexcept
    {
        return std::numeric_limits<std::size_t>::max() - (MAX_ALIGN - 1);
    }

    [[nodiscard]] void* allocate(std::size_t size)
    {
        if AGRPC_LIKELY (buffer_)
        {
            return buffer_.get();
        }
        else
        {
            buffer_.reset(new MaxAlignedData[MaxAlignedData::count(size)]);
            return buffer_.get();
        }
    }

  private:
    std::unique_ptr<MaxAlignedData[]> buffer_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_HPP
