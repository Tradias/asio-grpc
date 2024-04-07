// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_INTRUSIVE_CIRCULAR_LIST_HPP
#define AGRPC_DETAIL_INTRUSIVE_CIRCULAR_LIST_HPP

#include <iterator>
#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct IntrusiveCircularListHook
{
    IntrusiveCircularListHook* list_next_;
    IntrusiveCircularListHook* list_prev_;
};

class IntrusiveCircularList
{
  public:
    class iterator
    {
      public:
        using value_type = IntrusiveCircularListHook;
        using difference_type = std::ptrdiff_t;
        using reference = IntrusiveCircularListHook&;
        using pointer = IntrusiveCircularListHook*;
        using iterator_category = std::forward_iterator_tag;

        iterator() = default;

        explicit iterator(IntrusiveCircularListHook* item) : item_(item) {}

        reference operator*() noexcept { return *item_; }

        iterator& operator++() noexcept
        {
            item_ = item_->list_next_;
            return *this;
        }

        iterator operator++(int) noexcept
        {
            auto self = *this;
            item_ = item_->list_next_;
            return self;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.item_ == rhs.item_; }

        friend bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.item_ != rhs.item_; }

      private:
        IntrusiveCircularListHook* item_{};
    };

    IntrusiveCircularList() noexcept { clear(); }

    IntrusiveCircularList(const IntrusiveCircularList&) = delete;
    IntrusiveCircularList(IntrusiveCircularList&& other) = delete;
    IntrusiveCircularList& operator=(const IntrusiveCircularList&) = delete;
    IntrusiveCircularList& operator=(IntrusiveCircularList&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return node_.list_next_ == &node_; }

    [[nodiscard]] iterator begin() const noexcept { return iterator{node_.list_next_}; }

    [[nodiscard]] iterator end() noexcept { return iterator{&node_}; }

    void clear() noexcept
    {
        node_.list_next_ = &node_;
        node_.list_prev_ = &node_;
    }

    void push_front(IntrusiveCircularListHook* item) noexcept
    {
        auto* next = node_.list_next_;
        item->list_prev_ = &node_;
        item->list_next_ = next;
        node_.list_next_ = item;
        next->list_prev_ = item;
    }

    static IntrusiveCircularListHook* remove(IntrusiveCircularListHook* item) noexcept
    {
        auto* next = item->list_next_;
        auto* prev = item->list_prev_;
        prev->list_next_ = next;
        next->list_prev_ = prev;
        return next;
    }

  private:
    IntrusiveCircularListHook node_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_CIRCULAR_LIST_HPP
