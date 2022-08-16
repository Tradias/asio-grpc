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

#ifndef AGRPC_DETAIL_COPYABLE_ATOMIC_HPP
#define AGRPC_DETAIL_COPYABLE_ATOMIC_HPP

#include <agrpc/detail/config.hpp>

#include <atomic>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
class CopyableAtomic
{
  private:
    using Atomic = std::atomic<T>;

  public:
    CopyableAtomic() = default;

    explicit CopyableAtomic(T t) noexcept : value(t) {}

    CopyableAtomic(const CopyableAtomic& other) noexcept : CopyableAtomic(other.load()) {}

    CopyableAtomic& operator=(const CopyableAtomic& other) noexcept
    {
        value.store(other.load(), std::memory_order_relaxed);
        return *this;
    }

    [[nodiscard]] T load(std::memory_order memory_order = std::memory_order_relaxed) const noexcept
    {
        return value.load(memory_order);
    }

    [[nodiscard]] T exchange(T t, std::memory_order memory_order = std::memory_order_relaxed) noexcept
    {
        return value.exchange(t, memory_order);
    }

  private:
    Atomic value{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_COPYABLE_ATOMIC_HPP
