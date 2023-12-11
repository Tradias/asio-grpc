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

#ifndef AGRPC_DETAIL_EXECUTION_ASIO_HPP
#define AGRPC_DETAIL_EXECUTION_ASIO_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

#if defined(AGRPC_STANDALONE_ASIO) && ((ASIO_VERSION < 102500) || !defined(ASIO_NO_DEPRECATED))
#include <asio/execution/connect.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>

#define AGRPC_ASIO_HAS_SENDER_RECEIVER
#elif defined(AGRPC_BOOST_ASIO) && ((BOOST_VERSION < 108100) || !defined(BOOST_ASIO_NO_DEPRECATED))
#include <boost/asio/execution/connect.hpp>
#include <boost/asio/execution/set_done.hpp>
#include <boost/asio/execution/set_error.hpp>
#include <boost/asio/execution/set_value.hpp>
#include <boost/asio/execution/start.hpp>

#define AGRPC_ASIO_HAS_SENDER_RECEIVER
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct UnstoppableToken
{
    static constexpr bool stop_possible() noexcept { return false; }

    static constexpr bool stop_requested() noexcept { return false; }
};

namespace exec
{
template <class Object>
decltype(auto) get_allocator(const Object& object)
{
    return asio::get_associated_allocator(object);
}

struct GetSchedulerFn
{
    template <class Object>
    decltype(auto) operator()(const Object & object) const
    {
        return asio::get_associated_executor(object);
    }
};

inline constexpr GetSchedulerFn get_scheduler{};

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
using asio::execution::connect;
using asio::execution::connect_result_t;
using asio::execution::is_sender_v;
using asio::execution::set_done;
using asio::execution::set_error;
using asio::execution::set_value;
using asio::execution::start;
#else
template <class Sender>
inline constexpr bool is_sender_v = true;

template <class Sender, class Receiver>
auto connect(Sender&& sender, Receiver&& receiver)
{
    return static_cast<Sender&&>(sender).connect(static_cast<Receiver&&>(receiver));
}

template <class Sender, class Receiver>
using connect_result_t = decltype(exec::connect(std::declval<Sender>(), std::declval<Receiver>()));

template <class Receiver>
void set_done(Receiver&& receiver)
{
    static_cast<Receiver&&>(receiver).set_done();
}

template <class Receiver, class... T>
void set_error(Receiver&& receiver, T&&... t)
{
    static_cast<Receiver&&>(receiver).set_error(static_cast<T&&>(t)...);
}

template <class Receiver, class... T>
void set_value(Receiver&& receiver, T&&... t)
{
    static_cast<Receiver&&>(receiver).set_value(static_cast<T&&>(t)...);
}

template <class OperationState>
void start(OperationState&& state)
{
    static_cast<OperationState&&>(state).start();
}
#endif

template <class Receiver>
constexpr UnstoppableToken get_stop_token(const Receiver&) noexcept
{
    return {};
}

template <class>
using stop_token_type_t = UnstoppableToken;

template <class T, class = void>
inline constexpr bool stoppable_token = false;

template <class T>
inline constexpr bool stoppable_token<T, decltype((void)std::declval<T>().stop_possible())> = true;

template <class T, class = std::false_type>
inline constexpr bool unstoppable_token = false;

template <class T>
using UnstoppableTokenHelper = std::bool_constant<(T{}.stop_possible())>;

template <class T>
inline constexpr bool unstoppable_token<T, exec::UnstoppableTokenHelper<T>> = true;

namespace detail
{
template <class T>
constexpr T tag_t_helper(const T&);
}

template <auto& CPO>
using tag_t = decltype(detail::tag_t_helper(CPO));

struct inline_scheduler
{
};
}  // namespace exec
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_EXECUTION_ASIO_HPP
