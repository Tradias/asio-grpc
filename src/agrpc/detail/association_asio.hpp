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

#ifndef AGRPC_DETAIL_ASIO_ASSOCIATION_HPP
#define AGRPC_DETAIL_ASIO_ASSOCIATION_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
template <class Executor, class Function>
void do_execute(Executor&& executor, Function&& function)
{
    asio::execution::execute(static_cast<Executor&&>(executor), static_cast<Function&&>(function));
}
#else
template <class Executor, class Function>
void do_execute(Executor&& executor, Function&& function)
{
    static_cast<Executor&&>(executor).execute(static_cast<Function&&>(function));
}
#endif

template <class Executor, class Function, class Allocator>
void post_with_allocator(Executor&& executor, Function&& function, const Allocator& allocator)
{
    detail::do_execute(
        asio::prefer(asio::require(static_cast<Executor&&>(executor), asio::execution::blocking_t::never),
                     asio::execution::relationship_t::fork, asio::execution::allocator(allocator)),
        static_cast<Function&&>(function));
}

template <class CompletionHandler, class Function, class IOExecutor>
void complete_immediately(CompletionHandler&& completion_handler, Function&& function, const IOExecutor& io_executor)
{
    const auto allocator = asio::get_associated_allocator(completion_handler);
#ifdef AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR
    auto executor = asio::get_associated_immediate_executor(
        completion_handler,
        [&]() -> decltype(auto)
        {
            // Ensure that the io_executor is not already blocking::never because asio will try to convert const& to &&
            // due to chosing the `identity` overload of asio::require within the default_immediate_executor.
            if constexpr (asio::traits::static_require<IOExecutor, asio::execution::blocking_t::never_t>::is_valid)
            {
                return asio::prefer(io_executor, asio::execution::blocking_t::possibly);
            }
            else
            {
                return (io_executor);
            }
        }());
    detail::do_execute(
        asio::prefer(std::move(executor), asio::execution::allocator(allocator)),
        [ch = static_cast<CompletionHandler&&>(completion_handler), f = static_cast<Function&&>(function)]() mutable
        {
            static_cast<Function&&>(f)(static_cast<CompletionHandler&&>(ch));
        });
#else
    auto executor = asio::get_associated_executor(completion_handler, io_executor);
    detail::post_with_allocator(
        std::move(executor),
        [ch = static_cast<CompletionHandler&&>(completion_handler), f = static_cast<Function&&>(function)]() mutable
        {
            static_cast<Function&&>(f)(static_cast<CompletionHandler&&>(ch));
        },
        allocator);
#endif
}

template <class CancellationSlot>
bool stop_possible(CancellationSlot& cancellation_slot)
{
    return cancellation_slot.is_connected();
}

template <class T>
constexpr bool stop_requested(const T&) noexcept
{
    return false;
}

template <class CancellationSlot>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V = true;

template <>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V<detail::UnstoppableCancellationSlot> = false;

template <class T, class A = std::allocator<void>>
using AssociatedAllocatorT = asio::associated_allocator_t<T, A>;

template <class T, class E = asio::system_executor>
using AssociatedExecutorT = asio::associated_executor_t<T, E>;
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASIO_ASSOCIATION_HPP
