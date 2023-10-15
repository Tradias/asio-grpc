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

#ifndef AGRPC_DETAIL_ASSOCIATION_UNIFEX_HPP
#define AGRPC_DETAIL_ASSOCIATION_UNIFEX_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class StopToken>
constexpr bool stop_possible(StopToken& stop_token)
{
    return stop_token.stop_possible();
}

template <class StopToken>
constexpr bool stop_requested(StopToken& stop_token)
{
    return stop_token.stop_requested();
}

template <class T, class = std::false_type>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V = true;

template <class T>
using IsStopEverPossibleHelper = std::bool_constant<(T{}.stop_possible())>;

template <class T>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V<T, detail::IsStopEverPossibleHelper<T>> = false;

template <class T, class...>
using AssociatedAllocatorT = decltype(exec::get_allocator(std::declval<T>()));

template <class T, class...>
using AssociatedExecutorT = decltype(exec::get_executor(std::declval<T>()));
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASSOCIATION_UNIFEX_HPP
