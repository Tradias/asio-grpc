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

#ifndef AGRPC_DETAIL_SERVER_RPC_WITH_REQUEST_HPP
#define AGRPC_DETAIL_SERVER_RPC_WITH_REQUEST_HPP

#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/server_rpc_starter.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ServerRPC>
struct ServerRPCWithRequest
    : detail::ServerRPCRequestMessage<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>
{
    explicit ServerRPCWithRequest(const typename ServerRPC::executor_type& executor)
        : rpc_(detail::ServerRPCContextBaseAccess::construct<ServerRPC>(executor))
    {
    }

    ServerRPC rpc_;
};

template <class ServerRPC>
struct PickServerRPCPtrRequestMessage
{
    template <class, bool>
    using Type = detail::ServerRPCWithRequest<ServerRPC>;
};

template <class ServerRPC, class RPCHandler>
using ServerRPCPtrRequestMessageFactoryT =
    detail::RequestMessageFactoryServerRPCMixinT<PickServerRPCPtrRequestMessage<ServerRPC>::template Type, ServerRPC,
                                                 RPCHandler>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_WITH_REQUEST_HPP
