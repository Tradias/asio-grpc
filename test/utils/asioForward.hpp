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

#ifndef AGRPC_UTILS_ASIOFORWARD_HPP
#define AGRPC_UTILS_ASIOFORWARD_HPP

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/coroutine.hpp>
#include <asio/execution/allocator.hpp>
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
#endif

#ifdef ASIO_HAS_CONCEPTS
#define AGRPC_ASIO_HAS_CONCEPTS
#endif
#else
//
#include <boost/version.hpp>
//
#include <boost/asio/coroutine.hpp>
#include <boost/asio/execution/allocator.hpp>
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
#endif

#ifdef BOOST_ASIO_HAS_CONCEPTS
#define AGRPC_ASIO_HAS_CONCEPTS
#endif
#endif

namespace agrpc::test
{
#ifdef AGRPC_STANDALONE_ASIO
using ErrorCode = std::error_code;
#else
using ErrorCode = boost::system::error_code;
#endif
}  // namespace agrpc::test

#endif  // AGRPC_UTILS_ASIOFORWARD_HPP
