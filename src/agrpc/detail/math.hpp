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

#ifndef AGRPC_DETAIL_MATH_HPP
#define AGRPC_DETAIL_MATH_HPP

#include <agrpc/detail/config.hpp>

#include <climits>
#include <cstddef>

#if __cpp_lib_bitops >= 201907L
#include <bit>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
constexpr auto maximum(T a, T b) noexcept
{
    return (a < b) ? b : a;
}

#if __cpp_lib_bitops >= 201907L
constexpr std::size_t floor_log2(std::size_t x) noexcept
{
    constexpr auto SIZE_T_BIT_COUNT_MINUS_ONE = sizeof(std::size_t) * CHAR_BIT - std::size_t{1};
    return SIZE_T_BIT_COUNT_MINUS_ONE - std::countl_zero(x);
}
#else
inline constexpr std::size_t SIZE_T_BIT_COUNT = sizeof(std::size_t) * CHAR_BIT;
inline constexpr bool SIZE_T_BIT_COUNT_IS_POWER_OF_TWO = (SIZE_T_BIT_COUNT & (SIZE_T_BIT_COUNT - 1u)) == 0u;

constexpr std::size_t floor_log2_get_shift(std::size_t n) noexcept
{
    if constexpr (SIZE_T_BIT_COUNT_IS_POWER_OF_TWO)
    {
        return n >> 1;
    }
    else
    {
        return (n >> 1) + ((n & 1u) & static_cast<std::size_t>(n != 1u));
    }
}

inline constexpr auto FLOOR_LOG2_INITIAL_SHIFT = detail::floor_log2_get_shift(SIZE_T_BIT_COUNT);

constexpr std::size_t floor_log2(std::size_t x) noexcept
{
    std::size_t n = x;
    std::size_t log2 = 0;
    std::size_t shift = FLOOR_LOG2_INITIAL_SHIFT;
    while (shift != 0u)
    {
        if (std::size_t tmp = n >> shift; tmp != 0u)
        {
            log2 += shift;
            n = tmp;
        }
        shift = detail::floor_log2_get_shift(shift);
    }
    return log2;
}
#endif

constexpr bool is_pow2(std::size_t x) noexcept { return (x & (x - 1)) == 0; }

constexpr std::size_t ceil_log2(std::size_t x) noexcept
{
    return static_cast<std::size_t>(!detail::is_pow2(x)) + detail::floor_log2(x);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MATH_HPP
