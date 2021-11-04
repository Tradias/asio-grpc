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

#ifndef AGRPC_DETAIL_ASIOFORWARD_HPP
#define AGRPC_DETAIL_ASIOFORWARD_HPP

#ifdef AGRPC_STANDALONE_ASIO
//
#include <asio/version.hpp>
//
#include <asio/any_io_executor.hpp>
#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/execution/allocator.hpp>
#include <asio/execution/blocking.hpp>
#include <asio/execution/context.hpp>
#include <asio/execution/mapping.hpp>
#include <asio/execution/outstanding_work.hpp>
#include <asio/execution/relationship.hpp>
#include <asio/execution_context.hpp>
#include <asio/query.hpp>
#include <asio/use_awaitable.hpp>

#ifdef ASIO_HAS_CO_AWAIT
#include <boost/asio/use_awaitable.hpp>

#define AGRPC_ASIO_HAS_CO_AWAIT
#endif

#if (ASIO_VERSION >= 102000)
#include <asio/associated_cancellation_slot.hpp>

#define AGRPC_ASIO_HAS_CANCELLATION_SLOT
#endif
#elif defined(AGRPC_BOOST_ASIO)
//
#include <boost/version.hpp>
//
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/context.hpp>
#include <boost/asio/execution/mapping.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/query.hpp>
#include <boost/asio/use_awaitable.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/use_awaitable.hpp>

#define AGRPC_ASIO_HAS_CO_AWAIT
#endif

#if (BOOST_VERSION >= 107700)
#include <boost/asio/associated_cancellation_slot.hpp>

#define AGRPC_ASIO_HAS_CANCELLATION_SLOT
#endif
#endif

#ifdef AGRPC_UNIFEX
#include <unifex/config.hpp>
#include <unifex/receiver_concepts.hpp>
#endif

namespace agrpc
{
#ifdef AGRPC_STANDALONE_ASIO
namespace asio = ::asio;
#elif defined(AGRPC_BOOST_ASIO)
namespace asio = ::boost::asio;
#endif

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Object>
auto get_associated_executor_and_allocator(const Object& object)
{
    auto executor = asio::get_associated_executor(object);
    auto allocator = [&]
    {
        if constexpr (asio::can_query_v<decltype(executor), asio::execution::allocator_t<void>>)
        {
            return asio::get_associated_allocator(object, asio::query(executor, asio::execution::allocator));
        }
        else
        {
            return asio::get_associated_allocator(object);
        }
    }();
    return std::pair{std::move(executor), std::move(allocator)};
}

using asio::execution::set_done;
using asio::execution::set_error;
using asio::execution::set_value;
#elif defined(AGRPC_UNIFEX)
using unifex::set_done;
using unifex::set_error;
using unifex::set_value;
#endif
}  // namespace detail
}  // namespace agrpc

#endif  // AGRPC_DETAIL_ASIOFORWARD_HPP
