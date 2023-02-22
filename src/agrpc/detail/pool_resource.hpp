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
#include <agrpc/detail/math.hpp>
#include <agrpc/detail/memory.hpp>

#include <cstddef>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct MaxAlignAllocator
{
    static void* allocate(std::size_t size)
    {
        return std::allocator<MaxAlignedData>{}.allocate(MaxAlignedData::count(size));
    }

    static void deallocate(void* p, std::size_t size)
    {
        std::allocator<MaxAlignedData>{}.deallocate(static_cast<MaxAlignedData*>(p), MaxAlignedData::count(size));
    }
};

inline constexpr std::size_t POOL_OPTIONS_MINIMUM_MAX_BLOCKS_PER_CHUNK = 1u;
inline constexpr std::size_t POOL_OPTIONS_DEFAULT_MAX_BLOCKS_PER_CHUNK = 32u;
inline constexpr std::size_t POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK = MAX_ALIGN > 2 * sizeof(void*)
                                                                                    ? MAX_ALIGN
                                                                                    : 2 * sizeof(void*);
inline constexpr auto POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK_LOG2 =
    detail::ceil_log2(POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK);
inline constexpr std::size_t POOL_OPTIONS_DEFAULT_LARGEST_REQUIRED_POOL_BLOCK =
    POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK > 4096u ? POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK : 4096u;

struct list_node
{
    list_node* next_;
    list_node* prev_;
};

struct BlockListHeader : public list_node
{
    std::size_t size_;
};

inline void init_header(list_node* this_node) noexcept
{
    this_node->next_ = this_node;
    this_node->prev_ = this_node;
}

inline list_node* unlink(list_node* this_node) noexcept
{
    list_node* next = this_node->next_;
    list_node* prev = this_node->prev_;
    prev->next_ = next;
    next->prev_ = prev;
    return next;
}

inline void link_after(list_node* prev_node, list_node* this_node) noexcept
{
    list_node* next = prev_node->next_;
    this_node->prev_ = prev_node;
    this_node->next_ = next;
    prev_node->next_ = this_node;
    next->prev_ = this_node;
}

class BlockList
{
  public:
    static constexpr std::size_t HEADER_SIZE =
        std::size_t{sizeof(BlockListHeader) + MAX_ALIGN_MINUS_ONE} & std::size_t{~MAX_ALIGN_MINUS_ONE};

    BlockList() noexcept { detail::init_header(&list_); }

    BlockList(const BlockList&) = delete;
    BlockList(BlockList&&) = delete;
    BlockList operator=(const BlockList&) = delete;
    BlockList operator=(BlockList&&) = delete;

    void* allocate(std::size_t size)
    {
        void* p = MaxAlignAllocator::allocate(size + HEADER_SIZE);
        BlockListHeader& header = *::new (static_cast<void*>(p)) BlockListHeader;
        header.size_ = size + HEADER_SIZE;
        detail::link_after(&list_, &header);
        return (char*)p + HEADER_SIZE;
    }

    static void deallocate(void* p) noexcept
    {
        auto* header = reinterpret_cast<BlockListHeader*>(static_cast<char*>(p) - HEADER_SIZE);
        detail::unlink(header);
        const std::size_t size = header->size_;
        header->~BlockListHeader();
        MaxAlignAllocator::deallocate(header, size);
    }

    void release() noexcept
    {
        list_node* n = list_.next_;
        while (n != &list_)
        {
            auto& header = static_cast<BlockListHeader&>(*n);
            n = n->next_;
            std::size_t size = header.size_;
            header.~BlockListHeader();
            MaxAlignAllocator::deallocate(&header, size);
        }
        detail::init_header(&list_);
    }

  private:
    list_node list_;
};

struct slist_node
{
    slist_node* next_;
};

struct BlockSlistHeader : public slist_node
{
    std::size_t size_;
};

inline void init_header(slist_node* node) noexcept { node->next_ = nullptr; }

inline void link_after(slist_node* prev_node, slist_node* this_node) noexcept
{
    this_node->next_ = prev_node->next_;
    prev_node->next_ = this_node;
}

inline void unlink_after(slist_node* prev_node) noexcept
{
    const slist_node* this_node = prev_node->next_;
    prev_node->next_ = this_node->next_;
}

inline bool unique(const slist_node* this_node) noexcept
{
    slist_node* next = this_node->next_;
    return next == nullptr || next == this_node;
}

class BlockSlist
{
  public:
    static constexpr std::size_t HEADER_SIZE =
        std::size_t{sizeof(BlockSlistHeader) + MAX_ALIGN_MINUS_ONE} & std::size_t{~MAX_ALIGN_MINUS_ONE};

    BlockSlist() noexcept { detail::init_header(&slist_); }

    BlockSlist(const BlockSlist&) = delete;
    BlockSlist(BlockSlist&&) = delete;
    BlockSlist operator=(const BlockSlist&) = delete;
    BlockSlist operator=(BlockSlist&&) = delete;

    void* allocate(std::size_t size)
    {
        void* p = MaxAlignAllocator::allocate(size + HEADER_SIZE);
        auto& header = *::new (static_cast<void*>(p)) BlockSlistHeader;
        header.size_ = size + HEADER_SIZE;
        detail::link_after(&slist_, &header);
        return static_cast<char*>(p) + HEADER_SIZE;
    }

    void release() noexcept
    {
        slist_node* n = slist_.next_;
        while (n != nullptr)
        {
            auto& header = static_cast<BlockSlistHeader&>(*n);
            n = n->next_;
            std::size_t size = header.size_;
            header.~BlockSlistHeader();
            MaxAlignAllocator::deallocate(&header, size);
        }
        detail::init_header(&slist_);
    }

  private:
    slist_node slist_;
};

class Pool
{
  public:
    Pool() noexcept { detail::init_header(&free_slist_); }

    void* allocate_block() noexcept
    {
        if (detail::unique(&free_slist_))
        {
            return nullptr;
        }
        slist_node* pv = free_slist_.next_;
        detail::unlink_after(&free_slist_);
        pv->~slist_node();
        return pv;
    }

    void deallocate_block(void* p) noexcept
    {
        auto* pv = ::new (static_cast<void*>(p)) slist_node;
        detail::link_after(&free_slist_, pv);
    }

    void release() noexcept
    {
        detail::init_header(&free_slist_);
        block_slist_.release();
        next_blocks_per_chunk_ = POOL_OPTIONS_MINIMUM_MAX_BLOCKS_PER_CHUNK;
    }

    void replenish(std::size_t pool_block)
    {
        const std::size_t blocks_per_chunk = next_blocks_per_chunk_;

        // Minimum block size is at least max_align, so all pools allocate sizes that are multiple of max_align,
        // meaning that all blocks are max_align-aligned.
        auto* p = static_cast<char*>(block_slist_.allocate(blocks_per_chunk * pool_block));

        // Create header types. This is no-throw
        for (std::size_t i{}; i != blocks_per_chunk; ++i)
        {
            auto* const pv = ::new (static_cast<void*>(p)) slist_node;
            detail::link_after(&free_slist_, pv);
            p += pool_block;
        }

        // Update next block per chunk
        next_blocks_per_chunk_ = POOL_OPTIONS_DEFAULT_MAX_BLOCKS_PER_CHUNK / 2u < blocks_per_chunk
                                     ? POOL_OPTIONS_DEFAULT_MAX_BLOCKS_PER_CHUNK
                                     : blocks_per_chunk * 2u;
    }

    BlockSlist block_slist_;
    slist_node free_slist_;
    std::size_t next_blocks_per_chunk_{POOL_OPTIONS_MINIMUM_MAX_BLOCKS_PER_CHUNK};
};

constexpr std::size_t get_pool_index(std::size_t block_size)
{
    // For allocations equal or less than pool_options_minimum_largest_required_pool_block
    // the smallest pool is used
    block_size = detail::maximum(block_size, POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK);
    return detail::ceil_log2(block_size) - POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK_LOG2;
}

constexpr std::size_t get_block_size_of_pool_at(std::size_t index)
{
    // For allocations equal or less than pool_options_minimum_largest_required_pool_block
    // the smallest pool is used
    return POOL_OPTIONS_MINIMUM_LARGEST_REQUIRED_POOL_BLOCK << index;
}

inline constexpr std::size_t NUM_POOLS = detail::get_pool_index(POOL_OPTIONS_DEFAULT_LARGEST_REQUIRED_POOL_BLOCK) + 1u;

// Adapted from https://github.com/boostorg/container/blob/develop/src/pool_resource.cpp
class PoolResource
{
  public:
    PoolResource() = default;

    ~PoolResource() noexcept { release(); }

    PoolResource(const PoolResource& other) = delete;
    PoolResource(PoolResource&& other) = delete;
    PoolResource& operator=(const PoolResource& other) = delete;
    PoolResource& operator=(PoolResource&& other) = delete;

    void release() noexcept
    {
        oversized_list_.release();
        for (auto& pool : pools_)
        {
            pool.release();
        }
    }

    [[nodiscard]] void* allocate(std::size_t bytes, std::size_t /*alignment ignored, max_align is used by pools*/)
    {
        if (bytes > POOL_OPTIONS_DEFAULT_LARGEST_REQUIRED_POOL_BLOCK)
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
        if (bytes > POOL_OPTIONS_DEFAULT_LARGEST_REQUIRED_POOL_BLOCK)
        {
            return oversized_list_.deallocate(p);
        }
        const auto pool_idx = detail::get_pool_index(bytes);
        return pools_[pool_idx].deallocate_block(p);
    }

  private:
    BlockList oversized_list_;
    Pool pools_[NUM_POOLS];
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_POOL_RESOURCE_HPP
