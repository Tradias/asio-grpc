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

#ifndef AGRPC_DETAIL_ASSOCIATION_ASIO_HPP
#define AGRPC_DETAIL_ASSOCIATION_ASIO_HPP

#include <agrpc/detail/asio_utils.hpp>
#include <agrpc/detail/execution.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CancellationSlot>
inline constexpr bool IS_CANCELLATION_SLOT = !exec::stoppable_token<CancellationSlot>;

template <class T>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V = !exec::unstoppable_token<T>;

template <>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V<UncancellableSlot> = false;

template <class T>
inline constexpr bool IS_EXECUTOR = asio::is_executor<T>::value || asio::execution::is_executor_v<T>;

template <class T, class E = asio::system_executor>
using AssociatedExecutorT = asio::associated_executor_t<T, E>;

template <class T, class A = std::allocator<void>>
using AssociatedAllocatorT = asio::associated_allocator_t<T, A>;

template <class Object>
decltype(auto) get_executor(const Object& object) noexcept
{
    return asio::get_associated_executor(object);
}

template <class Object>
decltype(auto) get_allocator(const Object& object) noexcept
{
    return asio::get_associated_allocator(object);
}
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASSOCIATION_ASIO_HPP
