// Copyright 2025 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AGRPC_DETAIL_INTRUSIVE_STACK_HPP
#define AGRPC_DETAIL_INTRUSIVE_STACK_HPP

#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
class IntrusiveStack
{
  public:
    IntrusiveStack() = default;

    IntrusiveStack(const IntrusiveStack&) = delete;

    IntrusiveStack(IntrusiveStack&& other) noexcept : head_(std::exchange(other.head_, nullptr)) {}

    ~IntrusiveStack() = default;

    IntrusiveStack& operator=(const IntrusiveStack&) = delete;
    IntrusiveStack& operator=(IntrusiveStack&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    void push_front(T& item) noexcept
    {
        item.next_ = head_;
        head_ = &item;
    }

    [[nodiscard]] T& pop_front() noexcept
    {
        T* item = head_;
        head_ = item->next_;
        return *item;
    }

  private:
    T* head_{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_INTRUSIVE_STACK_HPP
