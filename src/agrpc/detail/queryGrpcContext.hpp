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

#ifndef AGRPC_DETAIL_QUERYGRPCCONTEXT_HPP
#define AGRPC_DETAIL_QUERYGRPCCONTEXT_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Executor>
agrpc::GrpcContext& query_grpc_context(const Executor& executor)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    if constexpr (asio::can_query_v<Executor, asio::execution::context_t>)
    {
        return static_cast<agrpc::GrpcContext&>(asio::query(executor, asio::execution::context));
    }
    else
#endif
    {
        return static_cast<agrpc::GrpcContext&>(executor.context());
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_QUERYGRPCCONTEXT_HPP
