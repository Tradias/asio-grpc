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

#ifndef AGRPC_UTILS_ASIOFORWARD_HPP
#define AGRPC_UTILS_ASIOFORWARD_HPP

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/coroutine.hpp>
#include <asio/execution.hpp>
#include <asio/post.hpp>
#include <asio/spawn.hpp>
#include <asio/steady_timer.hpp>
#include <asio/thread_pool.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
#include <asio/co_spawn.hpp>
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/experimental/deferred.hpp>
#include <asio/experimental/parallel_group.hpp>
#endif

#ifdef ASIO_HAS_CONCEPTS
#define AGRPC_ASIO_HAS_CONCEPTS
#endif
#elif defined(AGRPC_BOOST_ASIO)
//
#include <boost/version.hpp>
//
#include <boost/asio/coroutine.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
#include <boost/asio/co_spawn.hpp>
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/deferred.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#endif

#ifdef BOOST_ASIO_HAS_CONCEPTS
#define AGRPC_ASIO_HAS_CONCEPTS
#endif
#endif

#ifdef AGRPC_UNIFEX
#include <unifex/config.hpp>
#include <unifex/execute.hpp>
#include <unifex/just.hpp>
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

#if !UNIFEX_NO_COROUTINES
#include <unifex/task.hpp>
#endif
#endif

namespace test
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_UNIFEX)
using ErrorCode = std::error_code;
#elif defined(AGRPC_BOOST_ASIO)
using ErrorCode = boost::system::error_code;
#endif
}  // namespace test

#if defined(AGRPC_BOOST_ASIO)
namespace asio = ::boost::asio;
#endif

#endif  // AGRPC_UTILS_ASIOFORWARD_HPP
