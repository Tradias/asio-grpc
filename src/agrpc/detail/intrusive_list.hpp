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

#ifndef AGRPC_DETAIL_INTRUSIVE_LIST_HPP
#define AGRPC_DETAIL_INTRUSIVE_LIST_HPP

#include <iterator>
#include <utility>

#include <agrpc/detail/config.hpp>

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

        explicit iterator(T* item) : item_(item) {}

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
        T* item_{};
    };

    IntrusiveList() = default;

    IntrusiveList(const IntrusiveList&) = delete;

    IntrusiveList(IntrusiveList&& other) noexcept
        : head_(std::exchange(other.head_, nullptr)), tail_(std::exchange(other.tail_, nullptr))
    {
    }

    ~IntrusiveList() = default;

    IntrusiveList& operator=(const IntrusiveList&) = delete;
    IntrusiveList& operator=(IntrusiveList&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    [[nodiscard]] iterator begin() noexcept { return iterator{head_}; }

    [[nodiscard]] iterator end() noexcept { return iterator{}; }

    void push_back(T* item) noexcept
    {
        item->list_prev_ = tail_;
        item->list_next_ = nullptr;
        if (tail_ == nullptr)
        {
            head_ = item;
        }
        else
        {
            tail_->list_next_ = item;
        }
        tail_ = item;
    }

    void remove(T* item) noexcept
    {
        auto* const prev = item->list_prev_;
        auto* const next = item->list_next_;
        if (prev != nullptr)
        {
            prev->list_next_ = next;
        }
        else
        {
            head_ = next;
        }
        if (next != nullptr)
        {
            next->list_prev_ = prev;
        }
        else
        {
            tail_ = prev;
        }
    }

  private:
    T* head_{nullptr};
    T* tail_{nullptr};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_LIST_HPP
