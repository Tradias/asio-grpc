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

#ifndef AGRPC_AGRPC_REGISTER_SENDER_RPC_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_SENDER_RPC_HANDLER_HPP

#include <agrpc/detail/register_sender_rpc_handler.hpp>

AGRPC_NAMESPACE_BEGIN()

template <class ServerRPC, class RPCHandler>
[[nodiscard]] detail::RPCHandlerSender<ServerRPC, RPCHandler> register_sender_rpc_handler(
    agrpc::GrpcContext& grpc_context, detail::GetServerRPCServiceT<ServerRPC>& service, RPCHandler rpc_handler)
{
    return {grpc_context, service, static_cast<RPCHandler&&>(rpc_handler)};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REGISTER_SENDER_RPC_HANDLER_HPP
