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

#ifndef AGRPC_DETAIL_INTRUSIVE_SLIST_HPP
#define AGRPC_DETAIL_INTRUSIVE_SLIST_HPP

#include <iterator>
#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
class IntrusiveSlist
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
            item_ = item_->next_;
            return *this;
        }

        iterator operator++(int) noexcept
        {
            auto self = *this;
            item_ = item_->next_;
            return self;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.item_ == rhs.item_; }

        friend bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.item_ != rhs.item_; }

      private:
        T* item_{};
    };

    IntrusiveSlist() = default;

    IntrusiveSlist(const IntrusiveSlist&) = delete;

    IntrusiveSlist(IntrusiveSlist&& other) noexcept : head_(std::exchange(other.head_, nullptr)) {}

    ~IntrusiveSlist() = default;

    IntrusiveSlist& operator=(const IntrusiveSlist&) = delete;
    IntrusiveSlist& operator=(IntrusiveSlist&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    [[nodiscard]] iterator begin() noexcept { return iterator{head_}; }

    [[nodiscard]] iterator end() noexcept { return iterator{}; }

    void clear() noexcept { head_ = nullptr; }

    void push_front(T* item) noexcept
    {
        item->next_ = head_;
        head_ = item;
    }

    [[nodiscard]] T* pop_front() noexcept
    {
        auto* const current_head = head_;
        head_ = current_head->next_;
        return current_head;
    }

  private:
    T* head_{nullptr};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_SLIST_HPP
