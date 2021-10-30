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

#ifndef AGRPC_DETAIL_INTRUSIVEQUEUE_HPP
#define AGRPC_DETAIL_INTRUSIVEQUEUE_HPP

#include <utility>

namespace agrpc::detail
{
// Adapted from https://github.com/facebookexperimental/libunifex/blob/main/include/unifex/detail/intrusive_queue.hpp
template <class Item>
class IntrusiveQueue
{
  public:
    IntrusiveQueue() = default;

    IntrusiveQueue(IntrusiveQueue&& other) noexcept
        : head(std::exchange(other.head, nullptr)), tail(std::exchange(other.tail, nullptr))
    {
    }

    IntrusiveQueue& operator=(IntrusiveQueue&& other) noexcept
    {
        std::swap(head, other.head);
        std::swap(tail, other.tail);
        return *this;
    }

    [[nodiscard]] static IntrusiveQueue make_reversed(Item* list) noexcept
    {
        Item* new_head = nullptr;
        Item* new_tail = list;
        while (list != nullptr)
        {
            Item* next = list->next;
            list->next = new_head;
            new_head = list;
            list = next;
        }
        IntrusiveQueue result;
        result.head = new_head;
        result.tail = new_tail;
        return result;
    }

    [[nodiscard]] bool empty() const noexcept { return head == nullptr; }

    [[nodiscard]] Item* pop_front() noexcept
    {
        Item* item = std::exchange(head, head->next);
        if (head == nullptr)
        {
            tail = nullptr;
        }
        return item;
    }

    void push_back(Item* item) noexcept
    {
        item->next = nullptr;
        if (tail == nullptr)
        {
            head = item;
        }
        else
        {
            tail->next = item;
        }
        tail = item;
    }

    void append(IntrusiveQueue&& other) noexcept
    {
        if (other.empty()) return;
        auto* other_head = std::exchange(other.head, nullptr);
        if (empty())
        {
            head = other_head;
        }
        else
        {
            tail->next = other_head;
        }
        tail = std::exchange(other.tail, nullptr);
    }

  private:
    Item* head = nullptr;
    Item* tail = nullptr;
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_INTRUSIVEQUEUE_HPP
