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

#ifndef AGRPC_DETAIL_TAGGED_PTR_HPP
#define AGRPC_DETAIL_TAGGED_PTR_HPP

#include <agrpc/detail/config.hpp>

#include <cstdint>
#include <limits>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
[[nodiscard]] constexpr std::uintptr_t log2_ct(std::uintptr_t val) noexcept
{
    if (val == 1)
    {
        return 0;
    }
    std::uintptr_t ret{};
    while (val > 1)
    {
        val >>= 1;
        ret++;
    }
    return ret;
}

template <class T>
class TaggedPtr
{
  private:
    static constexpr std::uintptr_t AVAILABLE_BITS{detail::log2_ct(alignof(T))};
    static constexpr std::uintptr_t PTR_MASK{std::numeric_limits<std::uintptr_t>::max() & ~AVAILABLE_BITS};

  public:
    TaggedPtr() = default;

    explicit TaggedPtr(T* ptr) noexcept : ptr(reinterpret_cast<std::uintptr_t>(ptr)) {}

    explicit TaggedPtr(T& t) noexcept : TaggedPtr(std::addressof(t)) {}

    void clear() noexcept { ptr = std::uintptr_t{}; }

    [[nodiscard]] T* get() const noexcept { return reinterpret_cast<T*>(ptr & PTR_MASK); }

    [[nodiscard]] bool is_null() const noexcept { return (ptr & PTR_MASK) == std::uintptr_t{}; }

    [[nodiscard]] T* operator->() const noexcept { return this->get(); }

    [[nodiscard]] T& operator*() const noexcept { return *this->get(); }

    template <std::uintptr_t Bit>
    [[nodiscard]] bool has_bit() const noexcept
    {
        static_assert(Bit < AVAILABLE_BITS, "TaggedPtr has insufficient available bits");
        static constexpr auto SHIFT = std::uintptr_t{1} << Bit;
        return (ptr & SHIFT) != std::uintptr_t{};
    }

    template <std::uintptr_t Bit>
    void unset_bit() noexcept
    {
        static_assert(Bit < AVAILABLE_BITS, "TaggedPtr has insufficient available bits");
        static constexpr auto SHIFT = std::uintptr_t{1} << Bit;
        ptr &= ~SHIFT;
    }

    template <std::uintptr_t Bit>
    void set_bit() noexcept
    {
        static_assert(Bit < AVAILABLE_BITS, "TaggedPtr has insufficient available bits");
        static constexpr auto SHIFT = std::uintptr_t{1} << Bit;
        ptr |= SHIFT;
    }

  private:
    std::uintptr_t ptr{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_TAGGED_PTR_HPP
