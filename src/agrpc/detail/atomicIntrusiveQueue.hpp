// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_ATOMICINTRUSIVEQUEUE_HPP
#define AGRPC_DETAIL_ATOMICINTRUSIVEQUEUE_HPP

#include "agrpc/detail/intrusiveQueue.hpp"

#include <atomic>
#include <utility>

namespace agrpc::detail
{
// Adapted from
// https://github.com/facebookexperimental/libunifex/blob/main/include/unifex/detail/atomic_intrusive_queue.hpp
template <class Item>
class AtomicIntrusiveQueue
{
  public:
    AtomicIntrusiveQueue() = default;

    explicit AtomicIntrusiveQueue(bool initially_active) noexcept
        : head(initially_active ? nullptr : producer_inactive_value())
    {
    }

    AtomicIntrusiveQueue(const AtomicIntrusiveQueue&) = delete;
    AtomicIntrusiveQueue(AtomicIntrusiveQueue&&) = delete;
    AtomicIntrusiveQueue& operator=(const AtomicIntrusiveQueue&) = delete;
    AtomicIntrusiveQueue& operator=(AtomicIntrusiveQueue&&) = delete;

    // Enqueue an item to the queue.
    //
    // Returns true if the producer is inactive and needs to be
    // woken up. The calling thread has responsibility for waking
    // up the producer.
    [[nodiscard]] bool enqueue(Item* item) noexcept
    {
        void* const inactive = producer_inactive_value();
        void* old_value = head.load(std::memory_order_relaxed);
        do
        {
            item->next = (old_value == inactive) ? nullptr : static_cast<Item*>(old_value);
        } while (!head.compare_exchange_weak(old_value, item, std::memory_order_acq_rel));
        return old_value == inactive;
    }

    [[nodiscard]] detail::IntrusiveQueue<Item> dequeue_all() noexcept
    {
        void* value = head.load(std::memory_order_relaxed);
        if (value == nullptr)
        {
            return {};
        }
        value = head.exchange(nullptr, std::memory_order_acquire);
        return detail::IntrusiveQueue<Item>::make_reversed(static_cast<Item*>(value));
    }

    [[nodiscard]] bool try_mark_inactive() noexcept
    {
        void* const inactive = producer_inactive_value();
        void* old_value = head.load(std::memory_order_relaxed);
        if (old_value == nullptr)
        {
            if (head.compare_exchange_strong(old_value, inactive, std::memory_order_release, std::memory_order_relaxed))
            {
                // Successfully marked as inactive
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] detail::IntrusiveQueue<Item> try_mark_inactive_or_dequeue_all() noexcept
    {
        if (try_mark_inactive())
        {
            return {};
        }
        void* old_value = head.exchange(nullptr, std::memory_order_acquire);
        return detail::IntrusiveQueue<Item>::make_reversed(static_cast<Item*>(old_value));
    }

  private:
    void* producer_inactive_value() const noexcept
    {
        // Pick some pointer that is not nullptr and that is
        // guaranteed to not be the address of a valid item.
        return const_cast<void*>(static_cast<const void*>(&head));
    }

    std::atomic<void*> head = nullptr;
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_ATOMICINTRUSIVEQUEUE_HPP
