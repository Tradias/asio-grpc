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

#ifndef AGRPC_DETAIL_ASIO_FORWARD_HPP
#define AGRPC_DETAIL_ASIO_FORWARD_HPP

#include <agrpc/detail/config.hpp>

#ifdef AGRPC_STANDALONE_ASIO
//
#include <asio/version.hpp>
//
#include <asio/any_io_executor.hpp>
#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/bind_executor.hpp>
#include <asio/execution/allocator.hpp>
#include <asio/execution/blocking.hpp>
#include <asio/execution/connect.hpp>
#include <asio/execution/context.hpp>
#include <asio/execution/mapping.hpp>
#include <asio/execution/outstanding_work.hpp>
#include <asio/execution/relationship.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>
#include <asio/execution_context.hpp>
#include <asio/query.hpp>
#include <asio/system_executor.hpp>

#ifdef ASIO_HAS_CO_AWAIT
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>

#define AGRPC_ASIO_HAS_CO_AWAIT
#endif

#if (ASIO_VERSION >= 101900)
#include <asio/associated_cancellation_slot.hpp>
#include <asio/bind_cancellation_slot.hpp>

#define AGRPC_ASIO_HAS_CANCELLATION_SLOT
#endif

#if (ASIO_VERSION >= 102201)
#include <asio/bind_allocator.hpp>

#define AGRPC_ASIO_HAS_BIND_ALLOCATOR
#endif
#elif defined(AGRPC_BOOST_ASIO)
//
#include <boost/version.hpp>
//
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/connect.hpp>
#include <boost/asio/execution/context.hpp>
#include <boost/asio/execution/mapping.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/execution/set_done.hpp>
#include <boost/asio/execution/set_error.hpp>
#include <boost/asio/execution/set_value.hpp>
#include <boost/asio/execution/start.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/system_executor.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>

#define AGRPC_ASIO_HAS_CO_AWAIT
#endif

#if (BOOST_VERSION >= 107700)
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>

#define AGRPC_ASIO_HAS_CANCELLATION_SLOT
#endif

#if (BOOST_VERSION >= 107900)
#include <boost/asio/bind_allocator.hpp>

#define AGRPC_ASIO_HAS_BIND_ALLOCATOR
#endif
#endif

#ifdef AGRPC_UNIFEX
#include <unifex/config.hpp>
#include <unifex/get_allocator.hpp>
#include <unifex/get_stop_token.hpp>
#include <unifex/receiver_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/stop_token_concepts.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()
#ifdef AGRPC_STANDALONE_ASIO
namespace asio = ::asio;
#elif defined(AGRPC_BOOST_ASIO)
namespace asio = ::boost::asio;
#endif

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO)
using ErrorCode = std::error_code;
#elif defined(AGRPC_BOOST_ASIO)
using ErrorCode = boost::system::error_code;
#endif

template <class Slot>
class CancellationSlotAsStopToken;

namespace exec
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Object>
auto get_scheduler(const Object& object)
{
    return asio::get_associated_executor(object);
}

template <class Object>
auto get_executor(const Object& object)
{
    return asio::get_associated_executor(object);
}

template <class Object>
auto get_allocator(const Object& object)
{
    return asio::get_associated_allocator(object);
}

using asio::execution::connect;
using asio::execution::connect_result_t;
using asio::execution::is_sender_v;
using asio::execution::set_done;
using asio::execution::set_error;
using asio::execution::set_value;
using asio::execution::start;

struct unstoppable_token
{
    template <class F>
    struct callback_type
    {
        constexpr explicit callback_type(unstoppable_token, F&&) noexcept {}
    };

    [[nodiscard]] static constexpr bool stop_requested() noexcept { return false; }

    [[nodiscard]] static constexpr bool stop_possible() noexcept { return false; }
};

template <class Receiver>
auto get_stop_token([[maybe_unused]] Receiver&& receiver) noexcept
{
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    auto slot = asio::get_associated_cancellation_slot(static_cast<Receiver&&>(receiver), unstoppable_token{});
    if constexpr (std::is_same_v<unstoppable_token, decltype(slot)>)
    {
        return slot;
    }
    else
    {
        return detail::CancellationSlotAsStopToken<decltype(slot)>{std::move(slot)};
    }
#else
    return unstoppable_token{};
#endif
}

template <class>
using stop_token_type_t = detail::exec::unstoppable_token;
#elif defined(AGRPC_UNIFEX)
using ::unifex::get_allocator;
using ::unifex::get_scheduler;

template <class T>
inline constexpr bool is_sender_v = ::unifex::sender<T>;

using ::unifex::connect;
using ::unifex::connect_result_t;
using ::unifex::get_stop_token;
using ::unifex::set_done;
using ::unifex::set_error;
using ::unifex::set_value;
using ::unifex::start;
using ::unifex::stop_token_type_t;
using ::unifex::unstoppable_token;

template <class T, class = void>
inline constexpr bool HAS_GET_SCHEDULER = false;

template <class T>
inline constexpr bool HAS_GET_SCHEDULER<T, decltype((void)exec::get_scheduler(std::declval<T>()))> = true;

template <class Object>
auto get_executor(const Object& object)
{
    if constexpr (exec::HAS_GET_SCHEDULER<Object>)
    {
        return exec::get_scheduler(object);
    }
    else
    {
        return;
    }
}
#endif
}  // namespace exec

template <class T>
using AssociatedAllocatorT = decltype(detail::exec::get_allocator(std::declval<T>()));

template <class T>
using AssociatedExecutorT = decltype(detail::exec::get_executor(std::declval<T>()));

template <class Slot>
class CancellationSlotAsStopToken
{
  public:
    explicit CancellationSlotAsStopToken(Slot&& slot) : slot(static_cast<Slot&&>(slot)) {}

    template <class StopFunction>
    struct callback_type
    {
        explicit callback_type(CancellationSlotAsStopToken token, StopFunction&& function) noexcept
        {
            if (token.slot.is_connected())
            {
                token.slot.assign(static_cast<StopFunction&&>(function));
            }
        }
    };

    [[nodiscard]] static constexpr bool stop_requested() noexcept { return false; }

    [[nodiscard]] static constexpr bool stop_possible() noexcept { return true; }

  private:
    Slot slot;
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Executor, class Function, class Allocator>
void post_with_allocator(Executor&& executor, Function&& function, const Allocator& allocator)
{
    asio::execution::execute(
        asio::prefer(asio::require(static_cast<Executor&&>(executor), asio::execution::blocking_t::never),
                     asio::execution::relationship_t::fork, asio::execution::allocator(allocator)),
        static_cast<Function&&>(function));
}

struct UncancellableSlot
{
    template <class CancellationHandler, class... Args>
    static constexpr void emplace(Args&&...) noexcept
    {
    }

    template <class CancellationHandler>
    static constexpr void assign(CancellationHandler&&) noexcept
    {
    }

    static constexpr void clear() noexcept {}

    [[nodiscard]] static constexpr bool is_connected() noexcept { return false; }

    [[nodiscard]] static constexpr bool has_handler() noexcept { return false; }

    friend constexpr bool operator==(const UncancellableSlot&, const UncancellableSlot&) noexcept { return true; }

    friend constexpr bool operator!=(const UncancellableSlot&, const UncancellableSlot&) noexcept { return false; }
};

template <class Object, class Default = UncancellableSlot>
auto get_associated_cancellation_slot([[maybe_unused]] const Object& object,
                                      const Default& default_slot = Default{}) noexcept
{
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    return asio::get_associated_cancellation_slot(object, default_slot);
#else
    return default_slot;
#endif
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

template <class T>
inline constexpr bool IS_CANCEL_EVER_POSSIBLE_V = true;

template <>
inline constexpr bool IS_CANCEL_EVER_POSSIBLE_V<detail::UncancellableSlot> = false;
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASIO_FORWARD_HPP
