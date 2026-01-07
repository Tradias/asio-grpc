// Copyright 2026 Dennis Hezel
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

#include <agrpc/detail/intrusive_slist.hpp>
#include <agrpc/detail/math.hpp>
#include <agrpc/detail/memory.hpp>

#include <cstddef>

#include <agrpc/detail/config.hpp>

// The following PoolResource and related functions have been adapted from
// https://github.com/boostorg/container/blob/develop/src/pool_resource.cpp

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
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
    [[nodiscard]] void* allocate_already_max_aligned(std::size_t size)
    {
        const auto allocation_size = size + HEADER_SIZE;
        void* p = detail::allocate_already_max_aligned(allocation_size);
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
            detail::deallocate_already_max_aligned(&header, size);
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

    struct Header
    {
        bool unmanaged_{};
    };

    static constexpr std::size_t HEADER_SIZE = detail::align(sizeof(Header), MAX_ALIGN);
    static constexpr std::size_t MINIMUM_MAX_BLOCKS_PER_CHUNK = 1u;
    static constexpr std::size_t MAX_BLOCKS_PER_CHUNK = 32u;

  public:
    [[nodiscard]] void* allocate_block() noexcept
    {
        if (free_list_.empty())
        {
            return nullptr;
        }
        auto* pv = free_list_.pop_front();
        pv->~FreeListEntry();
        return pv;
    }

    [[nodiscard]] static void* allocate_unmanaged_block(std::size_t block_size) noexcept
    {
        const auto allocation_size = block_size + HEADER_SIZE;
        void* p = detail::allocate_already_max_aligned(allocation_size);
        auto* const header = ::new (p) Header;
        header->unmanaged_ = true;
        return static_cast<char*>(p) + HEADER_SIZE;
    }

    [[nodiscard]] bool max_size_reached() const noexcept { return next_blocks_per_chunk_ == MAX_BLOCKS_PER_CHUNK; }

    void deallocate_block(void* p, std::size_t block_size) noexcept
    {
        auto* const header = reinterpret_cast<Header*>(static_cast<char*>(p) - HEADER_SIZE);
        if (header->unmanaged_)
        {
            header->~Header();
            detail::deallocate_already_max_aligned(header, block_size + HEADER_SIZE);
        }
        else
        {
            auto* const pv = ::new (p) FreeListEntry;
            free_list_.push_front(pv);
        }
    }

    void release() noexcept
    {
        free_list_.clear();
        chunks_.release_already_max_aligned();
        next_blocks_per_chunk_ = MINIMUM_MAX_BLOCKS_PER_CHUNK;
    }

    void replenish(std::size_t block_size)
    {
        const auto blocks_per_chunk = next_blocks_per_chunk_;

        // Minimum block size is at least max_align, so all pools allocate sizes that are multiple of max_align,
        // meaning that all blocks are max_align-aligned.
        auto* p = static_cast<char*>(
            chunks_.allocate_already_max_aligned(blocks_per_chunk * block_size + blocks_per_chunk * HEADER_SIZE));

        for (std::size_t i{}; i != blocks_per_chunk; ++i)
        {
            ::new (static_cast<void*>(p)) Header{};
            auto* const pv = ::new (static_cast<void*>(p + HEADER_SIZE)) FreeListEntry;
            free_list_.push_front(pv);
            p += block_size + HEADER_SIZE;
        }

        next_blocks_per_chunk_ = blocks_per_chunk * 2u;
    }

  private:
    MemoryBlockSlist chunks_;
    FreeList free_list_;
    std::size_t next_blocks_per_chunk_{MINIMUM_MAX_BLOCKS_PER_CHUNK};
};

inline constexpr std::size_t MINIMUM_POOL_BLOCK_SIZE = MAX_ALIGN > sizeof(void*)
                                                           ? MAX_ALIGN
                                                           : detail::align(sizeof(void*), MAX_ALIGN);
inline constexpr std::size_t DESIRED_SMALLEST_POOL_BLOCK_SIZE = 32u;
inline constexpr std::size_t SMALLEST_POOL_BLOCK_SIZE =
    detail::align(DESIRED_SMALLEST_POOL_BLOCK_SIZE, MINIMUM_POOL_BLOCK_SIZE);
inline constexpr std::size_t SMALLEST_POOL_BLOCK_SIZE_LOG2 = detail::ceil_log2(SMALLEST_POOL_BLOCK_SIZE);
inline constexpr std::size_t LARGEST_POOL_BLOCK_SIZE =
    SMALLEST_POOL_BLOCK_SIZE > 1024u ? SMALLEST_POOL_BLOCK_SIZE : 1024u;

constexpr std::size_t get_pool_index(std::size_t size) noexcept
{
    // For allocations equal or less than SMALLEST_POOL_BLOCK_SIZE the smallest pool is used
    size = detail::maximum(size, SMALLEST_POOL_BLOCK_SIZE);
    return detail::ceil_log2(size) - SMALLEST_POOL_BLOCK_SIZE_LOG2;
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

    // Cannot handle allocations larger than LARGEST_POOL_BLOCK_SIZE or larger aligned than std::max_align_t
    [[nodiscard]] void* allocate(std::size_t size)
    {
        const auto pool_idx = detail::get_pool_index(size);
        Pool& pool = pools_[pool_idx];
        void* p = pool.allocate_block();
        if (p == nullptr)
        {
            const auto block_size = detail::get_block_size_of_pool_at(pool_idx);
            if (pool.max_size_reached())
            {
                p = pool.allocate_unmanaged_block(block_size);
            }
            else
            {
                pool.replenish(block_size);
                p = pool.allocate_block();
            }
        }
        return p;
    }

    void deallocate(void* p, std::size_t size)
    {
        const auto pool_idx = detail::get_pool_index(size);
        pools_[pool_idx].deallocate_block(p, detail::get_block_size_of_pool_at(pool_idx));
    }

    void release() noexcept
    {
        for (auto& pool : pools_)
        {
            pool.release();
        }
    }

  private:
    static constexpr std::size_t POOL_COUNT = detail::get_pool_index(LARGEST_POOL_BLOCK_SIZE) + 1u;

    Pool pools_[POOL_COUNT];
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POOL_RESOURCE_HPP
