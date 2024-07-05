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

#ifndef AGRPC_DETAIL_POOL_RESOURCE_HPP
#define AGRPC_DETAIL_POOL_RESOURCE_HPP

#include <agrpc/detail/math.hpp>
#include <agrpc/detail/memory.hpp>

#include <cstddef>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class Pool
{
  private:
    static constexpr std::size_t BLOCK_COUNT = 4;

  public:
    void* allocate_block(std::size_t pool_size)
    {
        for (auto& block : blocks)
        {
            if (block)
            {
                return std::exchange(block, nullptr);
            }
        }
        return MaxAlignAllocator::allocate_already_max_aligned(pool_size);
    }

    void deallocate_block(void* p, std::size_t pool_size) noexcept
    {
        for (auto& block : blocks)
        {
            if (!block)
            {
                block = p;
                return;
            }
        }
        MaxAlignAllocator::deallocate_already_max_aligned(p, pool_size);
    }

    void release(std::size_t pool_size) noexcept
    {
        for (auto& block : blocks)
        {
            if (block)
            {
                MaxAlignAllocator::deallocate_already_max_aligned(block, pool_size);
            }
        }
    }

  private:
    void* blocks[BLOCK_COUNT]{};
};

inline constexpr std::size_t SMALLEST_POOL_BLOCK_SIZE = 32u;
inline constexpr std::size_t SMALLEST_POOL_BLOCK_SIZE_LOG2 = detail::ceil_log2(SMALLEST_POOL_BLOCK_SIZE);
inline constexpr std::size_t LARGEST_POOL_BLOCK_SIZE = 512u;

constexpr std::size_t get_pool_index(std::size_t size) noexcept
{
    // For allocations equal or less than SMALLEST_POOL_BLOCK_SIZE the smallest pool is used
    size = detail::maximum(size, SMALLEST_POOL_BLOCK_SIZE);
    return detail::ceil_log2(size) - SMALLEST_POOL_BLOCK_SIZE_LOG2;
}

constexpr std::size_t get_pool_size(std::size_t pool_idx) noexcept { return SMALLEST_POOL_BLOCK_SIZE << pool_idx; }

struct PoolIndexAndSize
{
    std::size_t index;
    std::size_t size;
};

constexpr PoolIndexAndSize get_pool_index_and_size(std::size_t size) noexcept
{
    const auto pool_idx = detail::get_pool_index(size);
    return {pool_idx, detail::get_pool_size(pool_idx)};
}

class PoolResource
{
  public:
    PoolResource() = default;

    ~PoolResource() noexcept
    {
        auto pool_size = SMALLEST_POOL_BLOCK_SIZE;
        for (auto& pool : pools_)
        {
            pool.release(pool_size);
            pool_size <<= 1;
        }
    }

    PoolResource(const PoolResource& other) = delete;
    PoolResource(PoolResource&& other) = delete;
    PoolResource& operator=(const PoolResource& other) = delete;
    PoolResource& operator=(PoolResource&& other) = delete;

    [[nodiscard]] void* allocate(std::size_t size)
    {
        const auto [pool_idx, pool_size] = detail::get_pool_index_and_size(size);
        return pools_[pool_idx].allocate_block(pool_size);
    }

    void deallocate(void* p, std::size_t size)
    {
        const auto [pool_idx, pool_size] = detail::get_pool_index_and_size(size);
        pools_[pool_idx].deallocate_block(p, pool_size);
    }

  private:
    static constexpr std::size_t POOL_COUNT = detail::get_pool_index(LARGEST_POOL_BLOCK_SIZE) + 1u;

    Pool pools_[POOL_COUNT]{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POOL_RESOURCE_HPP
