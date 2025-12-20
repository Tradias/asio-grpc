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

#ifndef AGRPC_DETAIL_ASSOCIATION_EXECUTION_HPP
#define AGRPC_DETAIL_ASSOCIATION_EXECUTION_HPP

#include <agrpc/detail/execution.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CancellationSlot>
inline constexpr bool IS_CANCELLATION_SLOT = false;

template <class T>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V = !exec::unstoppable_token<T>;

template <class T>
inline constexpr bool IS_EXECUTOR = exec::scheduler<T>;

namespace assoc
{
template <class T, class...>
using associated_executor_t = decltype(exec::get_scheduler(std::declval<T>()));

template <class T, class...>
using associated_allocator_t = decltype(exec::get_allocator(std::declval<T>()));

inline const auto& get_associated_executor = exec::get_scheduler;

inline const auto& get_associated_allocator = exec::get_allocator;
}

template <class Executor>
inline constexpr bool IS_INLINE_EXECUTOR = std::is_same_v<exec::inline_scheduler, Executor>;
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASSOCIATION_EXECUTION_HPP
