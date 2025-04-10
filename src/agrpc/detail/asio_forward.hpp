// Copyright 2024 Dennis Hezel
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

#include <agrpc/detail/asio_macros.hpp>

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/any_io_executor.hpp>
#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/dispatch.hpp>
#include <asio/error.hpp>
#include <asio/execution/allocator.hpp>
#include <asio/execution/blocking.hpp>
#include <asio/execution/context.hpp>
#include <asio/execution/mapping.hpp>
#include <asio/execution/outstanding_work.hpp>
#include <asio/execution/relationship.hpp>
#include <asio/execution_context.hpp>
#include <asio/post.hpp>
#include <asio/query.hpp>
#include <asio/system_executor.hpp>

#ifdef ASIO_USE_TS_EXECUTOR_AS_DEFAULT
#include <boost/asio/executor_work_guard.hpp>
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <asio/associated_cancellation_slot.hpp>
#include <asio/bind_cancellation_slot.hpp>
#endif

#ifdef AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR
#include <asio/associated_immediate_executor.hpp>
#endif
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/context.hpp>
#include <boost/asio/execution/mapping.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/system_executor.hpp>

#ifdef BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
#include <boost/asio/executor_work_guard.hpp>
#endif

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#endif

#ifdef AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR
#include <boost/asio/associated_immediate_executor.hpp>
#endif
#else
#include <system_error>
#endif

#include <agrpc/detail/config.hpp>

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

#if defined(AGRPC_BOOST_ASIO) || defined(AGRPC_STANDALONE_ASIO)
inline auto operation_aborted_error_code() { return ErrorCode{asio::error::operation_aborted}; }
#else
inline auto operation_aborted_error_code() { return ErrorCode{}; }
#endif
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASIO_FORWARD_HPP
