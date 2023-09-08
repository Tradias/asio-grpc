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
#include <asio/error.hpp>
#include <asio/execution/allocator.hpp>
#include <asio/execution/blocking.hpp>
#include <asio/execution/context.hpp>
#include <asio/execution/mapping.hpp>
#include <asio/execution/outstanding_work.hpp>
#include <asio/execution/relationship.hpp>
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
#include <boost/asio/error.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/context.hpp>
#include <boost/asio/execution/mapping.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/execution/relationship.hpp>
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

#if (BOOST_VERSION >= 108000)
#define AGRPC_ASIO_HAS_NEW_SPAWN
#endif
#endif

#ifdef AGRPC_UNIFEX
#include <system_error>
#endif

AGRPC_NAMESPACE_BEGIN()

#ifdef AGRPC_STANDALONE_ASIO
namespace asio = ::asio;
#elif defined(AGRPC_BOOST_ASIO)
namespace asio = ::boost::asio;
#endif

namespace detail
{
#ifdef AGRPC_BOOST_ASIO
using ErrorCode = boost::system::error_code;
#else
using ErrorCode = std::error_code;
#endif

#ifdef AGRPC_UNIFEX
inline auto operation_aborted_error_code() { return ErrorCode{}; }
#else
inline auto operation_aborted_error_code() { return ErrorCode{asio::error::operation_aborted}; }
#endif
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASIO_FORWARD_HPP
