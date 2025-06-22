// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_QUERY_GRPC_CONTEXT_HPP
#define AGRPC_DETAIL_QUERY_GRPC_CONTEXT_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/forward.hpp>

#include <utility>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Context, class = void>
inline constexpr bool IS_CASTABLE_TO_GRPC_CONTEXT = false;

template <class Context>
inline constexpr bool
    IS_CASTABLE_TO_GRPC_CONTEXT<Context, decltype((void)static_cast<agrpc::GrpcContext&>(std::declval<Context&>()))> =
        true;

template <class Executor>
decltype(auto) query_execution_context(const Executor& executor)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (asio::can_query_v<Executor, asio::execution::context_t>)
    {
        return asio::query(executor, asio::execution::context);
    }
    else
#endif
    {
        return executor.context();
    }
}

template <class Executor>
agrpc::GrpcContext& query_grpc_context(const Executor& executor)
{
    auto&& context = detail::query_execution_context(executor);
    static_assert(detail::IS_CASTABLE_TO_GRPC_CONTEXT<decltype(context)>,
                  "Only GrpcExecutors or asio::any_io_executors created from such can be used.");
    return static_cast<agrpc::GrpcContext&>(context);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_QUERY_GRPC_CONTEXT_HPP
