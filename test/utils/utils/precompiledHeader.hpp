// Copyright 2021 Dennis Hezel
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

#include <doctest/doctest.h>
#include <grpcpp/alarm.h>
#include <grpcpp/grpcpp.h>

#ifdef AGRPC_STANDALONE_ASIO
//
#include <asio/version.hpp>
//
#include <asio/any_io_executor.hpp>
#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/coroutine.hpp>
#include <asio/execution.hpp>
#include <asio/execution/allocator.hpp>
#include <asio/execution/blocking.hpp>
#include <asio/execution/context.hpp>
#include <asio/execution/mapping.hpp>
#include <asio/execution/outstanding_work.hpp>
#include <asio/execution/relationship.hpp>
#include <asio/execution_context.hpp>
#include <asio/post.hpp>
#include <asio/query.hpp>
#include <asio/spawn.hpp>
#include <asio/steady_timer.hpp>
#include <asio/thread_pool.hpp>
#include <asio/use_awaitable.hpp>

#ifdef ASIO_HAS_CO_AWAIT
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#endif

#if (ASIO_VERSION >= 102000)
#include <asio/associated_cancellation_slot.hpp>
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/experimental/deferred.hpp>
#include <asio/experimental/parallel_group.hpp>
#endif
#elif defined(AGRPC_BOOST_ASIO)
//
#include <boost/asio/version.hpp>
//
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/context.hpp>
#include <boost/asio/execution/mapping.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#endif

#if (BOOST_ASIO_VERSION >= 102000)
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/deferred.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#endif
#elif defined(AGRPC_UNIFEX)
#include <unifex/config.hpp>
#include <unifex/execute.hpp>
#include <unifex/get_allocator.hpp>
#include <unifex/get_stop_token.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/new_thread_context.hpp>
#include <unifex/receiver_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/sequence.hpp>
#include <unifex/stop_token_concepts.hpp>
#include <unifex/stop_when.hpp>
#include <unifex/submit.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

#if !UNIFEX_NO_COROUTINES
#include <unifex/task.hpp>
#endif
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
