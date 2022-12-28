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

#ifndef AGRPC_DETAIL_ASIO_ASSOCIATION_HPP
#define AGRPC_DETAIL_ASIO_ASSOCIATION_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
using AssociatedAllocatorT = decltype(detail::exec::get_allocator(std::declval<T>()));

template <class T>
using AssociatedExecutorT = decltype(detail::exec::get_executor(std::declval<T>()));

template <class Slot>
class CancellationSlotAsStopToken
{
  public:
    explicit CancellationSlotAsStopToken(Slot&& slot) : slot_(static_cast<Slot&&>(slot)) {}

    template <class StopFunction>
    struct callback_type
    {
        template <class T>
        explicit callback_type(CancellationSlotAsStopToken token, T&& arg)
        {
            token.slot_.template emplace<StopFunction>(static_cast<T&&>(arg));
        }
    };

    [[nodiscard]] static constexpr bool stop_requested() noexcept { return false; }

    [[nodiscard]] bool stop_possible() const noexcept { return slot_.is_connected(); }

  private:
    Slot slot_;
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
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
#endif

template <class T>
using GetExecutorT = decltype(detail::exec::get_executor(std::declval<T>()));

template <class Receiver, class Callback>
using StopCallbackTypeT = typename detail::exec::stop_token_type_t<Receiver>::template callback_type<Callback>;

template <class T, class = std::false_type>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V = true;

template <class T>
using IsStopEverPossibleHelper = std::bool_constant<(T{}.stop_possible())>;

template <class T>
inline constexpr bool IS_STOP_EVER_POSSIBLE_V<T, detail::IsStopEverPossibleHelper<T>> = false;
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASIO_ASSOCIATION_HPP
