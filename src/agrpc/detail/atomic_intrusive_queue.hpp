// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_ATOMIC_INTRUSIVE_QUEUE_HPP
#define AGRPC_DETAIL_ATOMIC_INTRUSIVE_QUEUE_HPP

#include <agrpc/detail/intrusive_queue.hpp>

#include <atomic>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
// Adapted from
// https://github.com/facebookexperimental/libunifex/blob/main/include/unifex/detail/atomic_intrusive_queue.hpp
template <class Item>
class AtomicIntrusiveQueue
{
  public:
    AtomicIntrusiveQueue() = default;

    explicit AtomicIntrusiveQueue(bool initially_active) noexcept
        : head_(initially_active ? nullptr : producer_inactive_value())
    {
    }

    AtomicIntrusiveQueue(const AtomicIntrusiveQueue&) = delete;

    AtomicIntrusiveQueue(AtomicIntrusiveQueue&&) = delete;

    AtomicIntrusiveQueue& operator=(const AtomicIntrusiveQueue&) = delete;

    AtomicIntrusiveQueue& operator=(AtomicIntrusiveQueue&&) = delete;

    // Returns true if the previous state was inactive and this
    // operation successfully marked it as active.
    // Returns false if the previous state was active.
    [[nodiscard]] bool try_mark_active() noexcept
    {
        void* old_value = producer_inactive_value();
        return head_.compare_exchange_strong(old_value, nullptr, std::memory_order_acquire, std::memory_order_relaxed);
    }

    // Enqueue an item to the queue.
    //
    // Returns true if the producer is inactive and needs to be
    // woken up. The calling thread has responsibility for waking
    // up the producer.
    [[nodiscard]] bool enqueue(Item* item) noexcept
    {
        const void* const inactive = producer_inactive_value();
        void* old_value = head_.load(std::memory_order_relaxed);
        do
        {
            item->next_ = (old_value == inactive) ? nullptr : static_cast<Item*>(old_value);
        } while (!head_.compare_exchange_weak(old_value, item, std::memory_order_acq_rel));
        return old_value == inactive;
    }

    // Returns true if the producer is inactive and needs to be
    // woken up. The calling thread has responsibility for waking
    // up the producer.
    [[nodiscard]] bool prepend(IntrusiveQueue<Item> items) noexcept
    {
        if (items.empty())
        {
            return false;
        }
        const void* const inactive = producer_inactive_value();
        void* old_value = head_.load(std::memory_order_relaxed);
        do
        {
            items.tail_->next_ = (old_value == inactive) ? nullptr : static_cast<Item*>(old_value);
        } while (!head_.compare_exchange_weak(old_value, items.head_, std::memory_order_acq_rel));
        return old_value == inactive;
    }

    // Not valid to call if the producer is already marked as inactive.
    [[nodiscard]] bool dequeue_all_and_try_mark_inactive(detail::IntrusiveQueue<Item>& output) noexcept
    {
        void* const old_value = head_.exchange(nullptr, std::memory_order_acquire);
        void* const inactive = producer_inactive_value();
        void* expect_empty = nullptr;
        const bool marked_inactive =
            head_.compare_exchange_strong(expect_empty, inactive, std::memory_order_release, std::memory_order_relaxed);
        output.append(detail::IntrusiveQueue<Item>::make_reversed(static_cast<Item*>(old_value)));
        return marked_inactive;
    }

  private:
    [[nodiscard]] void* producer_inactive_value() const noexcept
    {
        // Pick some pointer that is not nullptr and that is
        // guaranteed to not be the address of a valid item.
        const void* head_address = &head_;
        return const_cast<void*>(head_address);
    }

    std::atomic<void*> head_{nullptr};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ATOMIC_INTRUSIVE_QUEUE_HPP
