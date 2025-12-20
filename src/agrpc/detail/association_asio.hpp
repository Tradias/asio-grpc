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

#ifndef AGRPC_DETAIL_ASSOCIATION_ASIO_HPP
#define AGRPC_DETAIL_ASSOCIATION_ASIO_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct UncancellableSlot
{
};

template <class CancellationSlot>
inline constexpr bool IS_CANCELLATION_SLOT = !exec::stoppable_token<CancellationSlot>;

template <class T>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V = !exec::unstoppable_token<T>;

template <>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V<UncancellableSlot> = false;

template <class T>
inline constexpr bool IS_EXECUTOR = asio::is_executor<T>::value || asio::execution::is_executor_v<T>;

namespace assoc
{
using asio::associated_executor_t;

using asio::associated_allocator_t;

using asio::get_associated_executor;

using asio::get_associated_allocator;
}

template <class Executor>
inline constexpr bool IS_INLINE_EXECUTOR = std::is_same_v<assoc::associated_executor_t<int>, Executor>;
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASSOCIATION_ASIO_HPP
