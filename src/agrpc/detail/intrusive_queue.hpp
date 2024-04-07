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

#ifndef AGRPC_DETAIL_INTRUSIVE_QUEUE_HPP
#define AGRPC_DETAIL_INTRUSIVE_QUEUE_HPP

#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
// Adapted from https://github.com/facebookexperimental/libunifex/blob/main/include/unifex/detail/intrusive_queue.hpp
template <class Item>
class IntrusiveQueue
{
  public:
    IntrusiveQueue() = default;

    IntrusiveQueue(const IntrusiveQueue&) = delete;

    IntrusiveQueue& operator=(const IntrusiveQueue&) = delete;

    IntrusiveQueue(IntrusiveQueue&& other) noexcept
        : head_(std::exchange(other.head_, nullptr)), tail_(std::exchange(other.tail_, nullptr))
    {
    }

    ~IntrusiveQueue() = default;

    IntrusiveQueue& operator=(IntrusiveQueue&& other) noexcept
    {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        return *this;
    }

    [[nodiscard]] static IntrusiveQueue make_reversed(Item* list) noexcept
    {
        Item* new_head = nullptr;
        Item* new_tail = list;
        while (list != nullptr)
        {
            Item* next = list->next_;
            list->next_ = new_head;
            new_head = list;
            list = next;
        }
        IntrusiveQueue result;
        result.head_ = new_head;
        result.tail_ = new_tail;
        return result;
    }

    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    [[nodiscard]] Item* pop_front() noexcept
    {
        Item* item = std::exchange(head_, head_->next_);
        if (head_ == nullptr)
        {
            tail_ = nullptr;
        }
        return item;
    }

    void push_back(Item* item) noexcept
    {
        item->next_ = nullptr;
        if (tail_ == nullptr)
        {
            head_ = item;
        }
        else
        {
            tail_->next_ = item;
        }
        tail_ = item;
    }

    void append(IntrusiveQueue other) noexcept
    {
        if (other.empty())
        {
            return;
        }
        auto* other_head = std::exchange(other.head_, nullptr);
        if (empty())
        {
            head_ = other_head;
        }
        else
        {
            tail_->next_ = other_head;
        }
        tail_ = std::exchange(other.tail_, nullptr);
    }

  private:
    Item* head_{nullptr};
    Item* tail_{nullptr};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_QUEUE_HPP
