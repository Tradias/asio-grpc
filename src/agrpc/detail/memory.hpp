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

#include <cstddef>
#include <memory>

#include <agrpc/detail/config.hpp>

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
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_HPP
