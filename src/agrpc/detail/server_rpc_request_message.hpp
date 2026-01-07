// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_DETAIL_SERVER_RPC_REQUEST_MESSAGE_HPP
#define AGRPC_DETAIL_SERVER_RPC_REQUEST_MESSAGE_HPP

#include <agrpc/rpc_type.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Request, bool NeedsRequestPtr>
struct ServerRPCRequestMessage
{
    static constexpr bool HAS_REQUEST_PTR = true;

    Request* request_;
};

template <class Request>
struct ServerRPCRequestMessage<Request, false>
{
    static constexpr bool HAS_REQUEST_PTR = false;
};

constexpr bool has_initial_request(agrpc::ServerRPCType type) noexcept
{
    return type == agrpc::ServerRPCType::SERVER_STREAMING || type == agrpc::ServerRPCType::UNARY;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_REQUEST_MESSAGE_HPP
