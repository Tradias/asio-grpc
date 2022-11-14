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

#include <iterator>
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
    class iterator
    {
      public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using pointer = T*;
        using iterator_category = std::forward_iterator_tag;

        iterator() = default;

        explicit iterator(T* item) : item(item) {}

        reference operator*() noexcept { return *item; }

        iterator& operator++() noexcept
        {
            item = item->list_next;
            return *this;
        }

        iterator operator++(int) noexcept
        {
            auto self = *this;
            item = item->list_next;
            return self;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.item == rhs.item; }

        friend bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.item != rhs.item; }

      private:
        T* item{};
    };

    IntrusiveList() = default;

    IntrusiveList(const IntrusiveList&) = delete;

    IntrusiveList(IntrusiveList&& other) noexcept
        : head(std::exchange(other.head, nullptr)), tail(std::exchange(other.tail, nullptr))
    {
    }

    ~IntrusiveList() = default;

    IntrusiveList& operator=(const IntrusiveList&) = delete;
    IntrusiveList& operator=(IntrusiveList&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return head == nullptr; }

    [[nodiscard]] iterator begin() noexcept { return iterator(head); }

    [[nodiscard]] iterator end() noexcept { return iterator(); }

    void push_back(T* item) noexcept
    {
        item->list_prev = tail;
        item->list_next = nullptr;
        if (tail == nullptr)
        {
            head = item;
        }
        else
        {
            tail->list_next = item;
        }
        tail = item;
    }

    [[nodiscard]] T* pop_front() noexcept
    {
        auto* item = head;
        head = item->list_next;
        if (head != nullptr)
        {
            head->list_prev = nullptr;
        }
        else
        {
            tail = nullptr;
        }
        return item;
    }

    void remove(T* item) noexcept
    {
        auto* const prev = item->list_prev;
        auto* const next = item->list_next;
        if (prev != nullptr)
        {
            prev->list_next = next;
        }
        else
        {
            head = next;
        }
        if (next != nullptr)
        {
            next->list_prev = prev;
        }
        else
        {
            tail = prev;
        }
    }

  private:
    T* head{nullptr};
    T* tail{nullptr};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_LIST_HPP
