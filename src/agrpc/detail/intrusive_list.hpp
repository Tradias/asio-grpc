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

#ifndef AGRPC_DETAIL_INTRUSIVE_LIST_HPP
#define AGRPC_DETAIL_INTRUSIVE_LIST_HPP

#include <agrpc/detail/config.hpp>

#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
// Adapted from
// https://github.com/facebookexperimental/libunifex/blob/main/include/unifex/detail/intrusive_list.hpp
template <class T>
class IntrusiveList
{
  public:
    IntrusiveList() noexcept : head(nullptr), tail(nullptr) {}

    IntrusiveList(const IntrusiveList&) = delete;

    IntrusiveList(IntrusiveList&& other) noexcept
        : head(std::exchange(other.head, nullptr)), tail(std::exchange(other.tail, nullptr))
    {
    }

    ~IntrusiveList() = default;

    IntrusiveList& operator=(const IntrusiveList&) = delete;
    IntrusiveList& operator=(IntrusiveList&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return head == nullptr; }

    void push_back(T* item) noexcept
    {
        item->prev = tail;
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

    [[nodiscard]] T* pop_front() noexcept
    {
        auto* item = head;
        head = item->next;
        if (head != nullptr)
        {
            head->prev = nullptr;
        }
        else
        {
            tail = nullptr;
        }
        return item;
    }

    void remove(T* item) noexcept
    {
        auto* const prev = item->prev;
        auto* const next = item->next;
        if (prev != nullptr)
        {
            prev->next = next;
        }
        else
        {
            head = next;
        }
        if (next != nullptr)
        {
            next->prev = prev;
        }
        else
        {
            tail = prev;
        }
    }

  private:
    T* head;
    T* tail;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_LIST_HPP
