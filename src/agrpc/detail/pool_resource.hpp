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

#ifndef AGRPC_DETAIL_POOL_RESOURCE_HPP
#define AGRPC_DETAIL_POOL_RESOURCE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/intrusive_circular_list.hpp>
#include <agrpc/detail/intrusive_slist.hpp>
#include <agrpc/detail/math.hpp>
#include <agrpc/detail/memory.hpp>

#include <cstddef>

// The following PoolResource and related functions have been adapted from
// https://github.com/boostorg/container/blob/develop/src/pool_resource.cpp

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class MemoryBlockList
{
  private:
    struct Header : detail::IntrusiveCircularListHook
    {
        std::size_t size_;
    };

    using List = detail::IntrusiveCircularList;

    static constexpr std::size_t HEADER_SIZE = detail::align(sizeof(Header), MAX_ALIGN);

  public:
    void* allocate(std::size_t size)
    {
        const auto allocation_size = size + HEADER_SIZE;
        void* p = MaxAlignAllocator::allocate(allocation_size);
        auto* const header = ::new (p) Header;
        header->size_ = allocation_size;
        list_.push_front(header);
        return static_cast<char*>(p) + HEADER_SIZE;
    }

    static void deallocate(void* p)
    {
        auto* header = reinterpret_cast<Header*>(static_cast<char*>(p) - HEADER_SIZE);
        List::remove(header);
        const auto size = header->size_;
        header->~Header();
        MaxAlignAllocator::deallocate(header, size);
    }

    void release() noexcept
    {
        for (auto it = list_.begin(); it != list_.end();)
        {
            auto& header = static_cast<Header&>(*it);
            ++it;
            std::size_t size = header.size_;
            header.~Header();
            MaxAlignAllocator::deallocate(&header, size);
        }
        list_.clear();
    }

  private:
    List list_;
};

class MemoryBlockSlist
{
  private:
    struct Header
    {
        Header* next_;
        std::size_t size_;
    };

    using List = detail::IntrusiveSlist<Header>;

    static constexpr std::size_t HEADER_SIZE = detail::align(sizeof(Header), MAX_ALIGN);

  public:
    void* allocate_already_max_aligned(std::size_t size)
    {
        const auto allocation_size = size + HEADER_SIZE;
        void* p = MaxAlignAllocator::allocate_already_max_aligned(allocation_size);
        auto* const header = ::new (p) Header;
        header->size_ = allocation_size;
        slist_.push_front(header);
        return static_cast<char*>(p) + HEADER_SIZE;
    }

    void release_already_max_aligned() noexcept
    {
        for (auto it = slist_.begin(); it != slist_.end();)
        {
            auto& header = *it;
            ++it;
            const auto size = header.size_;
            header.~Header();
            MaxAlignAllocator::deallocate_already_max_aligned(&header, size);
        }
        slist_.clear();
    }

  private:
    List slist_;
};

class Pool
{
  private:
    struct FreeListEntry
    {
        FreeListEntry* next_;
    };

    using FreeList = detail::IntrusiveSlist<FreeListEntry>;

    static constexpr std::size_t MINIMUM_MAX_BLOCKS_PER_CHUNK = 1u;
    static constexpr std::size_t MAX_BLOCKS_PER_CHUNK = 32u;

  public:
    void* allocate_block() noexcept
    {
        if (free_slist_.empty())
        {
            return nullptr;
        }
        auto* pv = free_slist_.pop_front();
        pv->~FreeListEntry();
        return pv;
    }

    void deallocate_block(void* p) noexcept
    {
        auto* pv = ::new (p) FreeListEntry;
        free_slist_.push_front(pv);
    }

    void release() noexcept
    {
        free_slist_.clear();
        block_slist_.release_already_max_aligned();
        next_blocks_per_chunk_ = MINIMUM_MAX_BLOCKS_PER_CHUNK;
    }

    void replenish(std::size_t block_size)
    {
        const std::size_t blocks_per_chunk = next_blocks_per_chunk_;

        // Minimum block size is at least max_align, so all pools allocate sizes that are multiple of max_align,
        // meaning that all blocks are max_align-aligned.
        auto* p = static_cast<char*>(block_slist_.allocate_already_max_aligned(blocks_per_chunk * block_size));

        for (std::size_t i{}; i != blocks_per_chunk; ++i)
        {
            auto* const pv = ::new (static_cast<void*>(p)) FreeListEntry;
            free_slist_.push_front(pv);
            p += block_size;
        }

        next_blocks_per_chunk_ =
            MAX_BLOCKS_PER_CHUNK / 2u < blocks_per_chunk ? MAX_BLOCKS_PER_CHUNK : blocks_per_chunk * 2u;
    }

  private:
    MemoryBlockSlist block_slist_;
    FreeList free_slist_;
    std::size_t next_blocks_per_chunk_{MINIMUM_MAX_BLOCKS_PER_CHUNK};
};

inline constexpr std::size_t POWER_OF_TWO_SIZEOF_VOID_PTR = (sizeof(void*) % 2 == 0) ? sizeof(void*) : MAX_ALIGN;
inline constexpr std::size_t SMALLEST_POOL_BLOCK_SIZE =
    MAX_ALIGN > 2 * POWER_OF_TWO_SIZEOF_VOID_PTR ? MAX_ALIGN : 2 * POWER_OF_TWO_SIZEOF_VOID_PTR;
inline constexpr std::size_t SMALLEST_POOL_BLOCK_SIZE_LOG2 = detail::ceil_log2(SMALLEST_POOL_BLOCK_SIZE);
inline constexpr std::size_t LARGEST_POOL_BLOCK_SIZE =
    SMALLEST_POOL_BLOCK_SIZE > 4096u ? SMALLEST_POOL_BLOCK_SIZE : 4096u;

constexpr std::size_t get_pool_index(std::size_t block_size) noexcept
{
    // For allocations equal or less than SMALLEST_POOL_BLOCK_SIZE the smallest pool is used
    block_size = detail::maximum(block_size, SMALLEST_POOL_BLOCK_SIZE);
    return detail::ceil_log2(block_size) - SMALLEST_POOL_BLOCK_SIZE_LOG2;
}

constexpr std::size_t get_block_size_of_pool_at(std::size_t index) noexcept
{
    return SMALLEST_POOL_BLOCK_SIZE << index;
}

class PoolResource
{
  public:
    PoolResource() = default;

    ~PoolResource() noexcept { release(); }

    PoolResource(const PoolResource& other) = delete;
    PoolResource(PoolResource&& other) = delete;
    PoolResource& operator=(const PoolResource& other) = delete;
    PoolResource& operator=(PoolResource&& other) = delete;

    [[nodiscard]] void* allocate(std::size_t bytes, std::size_t /*alignment ignored, max_align is used by pools*/)
    {
        if (bytes > LARGEST_POOL_BLOCK_SIZE)
        {
            return oversized_list_.allocate(bytes);
        }
        const auto pool_idx = detail::get_pool_index(bytes);
        Pool& pool = pools_[pool_idx];
        void* p = pool.allocate_block();
        if (p == nullptr)
        {
            pool.replenish(detail::get_block_size_of_pool_at(pool_idx));
            p = pool.allocate_block();
        }
        return p;
    }

    void deallocate(void* p, std::size_t bytes, std::size_t /*alignment ignored, max_align is used by pools*/)
    {
        if (bytes > LARGEST_POOL_BLOCK_SIZE)
        {
            return MemoryBlockList::deallocate(p);
        }
        const auto pool_idx = detail::get_pool_index(bytes);
        return pools_[pool_idx].deallocate_block(p);
    }

    void release() noexcept
    {
        oversized_list_.release();
        for (auto& pool : pools_)
        {
            pool.release();
        }
    }

  private:
    static constexpr std::size_t POOL_COUNT = detail::get_pool_index(LARGEST_POOL_BLOCK_SIZE) + 1u;

    MemoryBlockList oversized_list_;
    Pool pools_[POOL_COUNT];
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POOL_RESOURCE_HPP
