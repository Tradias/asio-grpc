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

#ifndef AGRPC_UTILS_INLINE_EXECUTOR_HPP
#define AGRPC_UTILS_INLINE_EXECUTOR_HPP

#include <agrpc/detail/asio_forward.hpp>

namespace test
{
struct InlineExecutor
{
    friend bool operator==(const InlineExecutor&, const InlineExecutor&) noexcept { return true; }

    friend bool operator!=(const InlineExecutor&, const InlineExecutor&) noexcept { return false; }

    template <typename Function>
    void execute(Function&& f) const
    {
        static_cast<Function&&>(f)();
    }
};
}  // namespace test

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)
template <>
struct agrpc::asio::traits::equality_comparable<test::InlineExecutor>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)
template <class F>
struct agrpc::asio::traits::execute_member<test::InlineExecutor, F>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
};
#endif

#endif  // AGRPC_UTILS_INLINE_EXECUTOR_HPP
