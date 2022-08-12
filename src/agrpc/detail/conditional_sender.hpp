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
#include <agrpc/detail/utility.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Sender>
class ConditionalSender;

struct ConditionalSenderAccess
{
    template <class Sender>
    static auto create(Sender&& sender, bool condition)
    {
        return detail::ConditionalSender<detail::RemoveCrefT<Sender>>(std::forward<Sender>(sender), condition);
    }
};

template <class Sender, class Receiver>
class ConditionalSenderOperationState;

template <class Sender>
class ConditionalSender
{
  public:
    template <template <class...> class Variant, template <class...> class Tuple>
    using value_types = typename Sender::template value_types<Variant, Tuple>;

    template <template <class...> class Variant>
    using error_types = typename Sender::template error_types<Variant>;

    static constexpr bool sends_done = Sender::sends_done;

    template <class Receiver>
    detail::ConditionalSenderOperationState<Sender, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) && noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                          std::is_nothrow_move_constructible_v<Sender>))
    {
        return {std::forward<Receiver>(receiver), std::move(sender), condition};
    }

    template <class Receiver, class S = Sender, class = std::enable_if_t<std::is_copy_constructible_v<S>>>
    detail::ConditionalSenderOperationState<Sender, detail::RemoveCrefT<Receiver>> connect(
        Receiver&& receiver) const& noexcept((detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver> &&
                                              std::is_nothrow_copy_constructible_v<Sender>))
    {
        return {std::forward<Receiver>(receiver), sender, condition};
    }

  private:
    friend detail::ConditionalSenderAccess;

    ConditionalSender(Sender&& sender, bool condition) : sender{std::move(sender)}, condition(condition) {}

    Sender sender;
    bool condition;
};

template <class Variant>
struct ConditionalSenderSatisfyReceiver;

template <class... T, class... Tuple>
struct ConditionalSenderSatisfyReceiver<detail::TypeList<detail::TypeList<T...>, Tuple...>>
{
    template <class Receiver>
    static auto satisfy(Receiver&& receiver)
    {
        detail::satisfy_receiver(std::forward<Receiver>(receiver), T{}...);
    }
};

template <class Sender, class Receiver>
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
            detail::ConditionalSenderSatisfyReceiver<CompletionValues>::satisfy(std::move(operation_state.receiver()));
        }
    }

  private:
    friend detail::ConditionalSender<Sender>;

    template <class R>
    ConditionalSenderOperationState(R&& receiver, Sender&& sender, bool condition)
        : operation_state(detail::exec::connect(std::move(sender), std::forward<R>(receiver))), condition(condition)
    {
    }

    template <class R>
    ConditionalSenderOperationState(R&& receiver, const Sender& sender, bool condition)
        : operation_state(detail::exec::connect(sender, std::forward<R>(receiver))), condition(condition)
    {
    }

    detail::exec::connect_result_t<Sender, Receiver> operation_state;
    bool condition;
};
}

AGRPC_NAMESPACE_END

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)
template <class Sender, class R>
struct agrpc::asio::traits::connect_member<agrpc::detail::ConditionalSender<Sender>, R>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept =
        noexcept(std::declval<agrpc::detail::ConditionalSender<Sender>>().connect(std::declval<R>()));

    using result_type = decltype(std::declval<agrpc::detail::ConditionalSender<Sender>>().connect(std::declval<R>()));
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_START_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)
template <class Sender, class Receiver>
struct agrpc::asio::traits::start_member<agrpc::detail::ConditionalSenderOperationState<Sender, Receiver>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};
#endif

#endif  // AGRPC_DETAIL_CONDITIONAL_SENDER_HPP
