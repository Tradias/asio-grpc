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

#ifndef AGRPC_DETAIL_CONDITIONAL_SENDER_HPP
#define AGRPC_DETAIL_CONDITIONAL_SENDER_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/tuple.hpp>
#include <agrpc/detail/utility.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Sender, class... CompletionArgs>
class ConditionalSender;

struct ConditionalSenderAccess
{
    template <class Sender, class... CompletionArgs>
    static auto create(Sender&& sender, bool condition, CompletionArgs&&... args)
    {
        return detail::ConditionalSender(static_cast<Sender&&>(sender), condition,
                                         static_cast<CompletionArgs&&>(args)...);
    }
};

template <class Sender, class Receiver, class... CompletionArgs>
class ConditionalSenderOperationState;

template <class Sender, class... CompletionArgs>
class ConditionalSender
{
  public:
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = typename Sender::template value_types<Variant, Tuple>;

    template <template <class...> class Variant>
    using error_types = typename Sender::template error_types<Variant>;

    static constexpr bool sends_done = Sender::sends_done;

    template <class Receiver>
    detail::ConditionalSenderOperationState<Sender, detail::RemoveCrefT<Receiver>, CompletionArgs...> connect(
        Receiver&& receiver) && noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                          std::is_nothrow_move_constructible_v<Sender>))
    {
        return {static_cast<Receiver&&>(receiver), std::move(sender), condition, std::move(args)};
    }

    template <class Receiver, class S = Sender, class = std::enable_if_t<std::is_copy_constructible_v<S>>>
    detail::ConditionalSenderOperationState<Sender, detail::RemoveCrefT<Receiver>, CompletionArgs...> connect(
        Receiver&& receiver) const& noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                              std::is_nothrow_copy_constructible_v<Sender>))
    {
        return {static_cast<Receiver&&>(receiver), sender, condition, args};
    }

  private:
    friend detail::ConditionalSenderAccess;

    ConditionalSender(Sender&& sender, bool condition, CompletionArgs&&... args)
        : sender{std::move(sender)}, args{std::move(args)...}, condition(condition)
    {
    }

    Sender sender;
    detail::Tuple<CompletionArgs...> args;
    bool condition;
};

template <class Sender, class... CompletionArgs>
ConditionalSender(Sender, CompletionArgs...) -> ConditionalSender<Sender, CompletionArgs...>;

template <class Variant>
struct ConditionalSenderSatisfyReceiver;

template <class... T, class... Tuple>
struct ConditionalSenderSatisfyReceiver<detail::TypeList<detail::TypeList<T...>, Tuple...>>
{
    template <class Receiver, class... Args, size_t... I>
    static auto satisfy_impl(Receiver&& receiver, detail::Tuple<Args...>&& tuple, std::index_sequence<I...>)
    {
        detail::satisfy_receiver(static_cast<Receiver&&>(receiver), detail::get<I>(std::move(tuple))...);
    }

    template <class Receiver, class... Args>
    static auto satisfy(Receiver&& receiver, detail::Tuple<Args...>&& tuple)
    {
        ConditionalSenderSatisfyReceiver::satisfy_impl(static_cast<Receiver&&>(receiver), std::move(tuple),
                                                       std::make_index_sequence<sizeof...(Args)>{});
    }

    template <class Receiver>
    static auto satisfy(Receiver&& receiver, detail::Tuple<>&&)
    {
        detail::satisfy_receiver(static_cast<Receiver&&>(receiver), T{}...);
    }
};

template <class Sender, class Receiver, class... CompletionArgs>
class ConditionalSenderOperationState
{
  public:
    void start() noexcept
    {
        if (condition)
        {
            detail::exec::start(operation_state);
        }
        else
        {
            using CompletionValues = typename Sender::template value_types<detail::TypeList, detail::TypeList>;
            detail::ConditionalSenderSatisfyReceiver<CompletionValues>::satisfy(std::move(operation_state.receiver()),
                                                                                std::move(args));
        }
    }

  private:
    friend detail::ConditionalSender<Sender, CompletionArgs...>;

    template <class R>
    ConditionalSenderOperationState(R&& receiver, Sender&& sender, bool condition,
                                    detail::Tuple<CompletionArgs...>&& args)
        : operation_state(detail::exec::connect(std::move(sender), static_cast<R&&>(receiver))),
          args(std::move(args)),
          condition(condition)
    {
    }

    template <class R>
    ConditionalSenderOperationState(R&& receiver, const Sender& sender, bool condition,
                                    const detail::Tuple<CompletionArgs...>& args)
        : operation_state(detail::exec::connect(sender, static_cast<R&&>(receiver))), args(args), condition(condition)
    {
    }

    detail::exec::connect_result_t<Sender, Receiver> operation_state;
    detail::Tuple<CompletionArgs...> args;
    bool condition;
};
}

AGRPC_NAMESPACE_END

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)
template <class Sender, class Receiver, class... CompletionArgs>
struct agrpc::asio::traits::connect_member<agrpc::detail::ConditionalSender<Sender, CompletionArgs...>, Receiver>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = noexcept(
        std::declval<agrpc::detail::ConditionalSender<Sender, CompletionArgs...>>().connect(std::declval<Receiver>()));

    using result_type = decltype(std::declval<agrpc::detail::ConditionalSender<Sender, CompletionArgs...>>().connect(
        std::declval<Receiver>()));
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)
template <class Sender, class Receiver, class... CompletionArgs>
struct agrpc::asio::traits::start_member<
    agrpc::detail::ConditionalSenderOperationState<Sender, Receiver, CompletionArgs...>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};
#endif

#endif  // AGRPC_DETAIL_CONDITIONAL_SENDER_HPP
