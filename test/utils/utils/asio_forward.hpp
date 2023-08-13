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

#ifndef AGRPC_UTILS_ASIO_FORWARD_HPP
#define AGRPC_UTILS_ASIO_FORWARD_HPP

#include <agrpc/detail/asio_forward.hpp>

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/coroutine.hpp>
#include <asio/error.hpp>
#include <asio/execution.hpp>
#include <asio/execution/submit.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/spawn.hpp>
#include <asio/steady_timer.hpp>
#include <asio/thread_pool.hpp>
#include <asio/use_future.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
#include <asio/co_spawn.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <asio/experimental/awaitable_operators.hpp>
#endif
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/experimental/parallel_group.hpp>
#endif

#ifdef ASIO_HAS_CONCEPTS
#define AGRPC_TEST_ASIO_HAS_CONCEPTS
#endif

#if (ASIO_VERSION >= 102200)
#define AGRPC_TEST_ASIO_HAS_FIXED_DEFERRED
#endif

#if (ASIO_VERSION >= 102400)
#include <asio/deferred.hpp>

#define AGRPC_TEST_ASIO_HAS_NEW_SPAWN
#elif defined(AGRPC_ASIO_HAS_CANCELLATION_SLOT)
#include <asio/experimental/deferred.hpp>
#endif
#elif defined(AGRPC_BOOST_ASIO)
//
#include <boost/version.hpp>
//
#include <boost/asio/coroutine.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/execution/submit.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
#include <boost/asio/co_spawn.hpp>

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <boost/asio/experimental/awaitable_operators.hpp>
#endif
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#endif

#ifdef BOOST_ASIO_HAS_CONCEPTS
#define AGRPC_TEST_ASIO_HAS_CONCEPTS
#endif

#if (BOOST_VERSION >= 107800)
#define AGRPC_TEST_ASIO_HAS_FIXED_DEFERRED
#endif

#if (BOOST_VERSION >= 108000)
#include <boost/asio/deferred.hpp>

#define AGRPC_TEST_ASIO_HAS_NEW_SPAWN
#elif defined(AGRPC_ASIO_HAS_CANCELLATION_SLOT)
#include <boost/asio/experimental/deferred.hpp>
#endif
#endif

#ifdef AGRPC_UNIFEX
#include <unifex/config.hpp>
#include <unifex/finally.hpp>
#include <unifex/just.hpp>
#include <unifex/let_done.hpp>
#include <unifex/let_error.hpp>
#include <unifex/let_value.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/let_value_with_stop_source.hpp>
#include <unifex/new_thread_context.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/sequence.hpp>
#include <unifex/stop_when.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>
#include <unifex/with_query_value.hpp>

#if !UNIFEX_NO_COROUTINES
#include <unifex/task.hpp>
#endif
#endif

#if defined(AGRPC_BOOST_ASIO)
namespace asio = ::boost::asio;
#endif

namespace test
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_UNIFEX)
using ErrorCode = std::error_code;
#elif defined(AGRPC_BOOST_ASIO)
using ErrorCode = boost::system::error_code;
#endif

#ifdef AGRPC_TEST_ASIO_HAS_NEW_SPAWN
inline constexpr auto ASIO_DEFERRED = asio::deferred;
#elif defined(AGRPC_ASIO_HAS_CANCELLATION_SLOT)
inline constexpr auto ASIO_DEFERRED = asio::experimental::deferred;
#endif
}  // namespace test

#endif  // AGRPC_UTILS_ASIO_FORWARD_HPP
