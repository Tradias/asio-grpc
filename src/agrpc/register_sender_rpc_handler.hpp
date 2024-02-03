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

/**
 * @brief Register an sender rpc handler for the given method
 *
 * The rpc handler will be invoked for every incoming request of this gRPC method. It must take `ServerRPC&` as
 * first argument and `ServerRPC::Request&` as second argument (only for unary and server-streaming rpcs). The ServerRPC
 * is automatically cancelled at the end of the rpc handler's sender if `finish()` was not called earlier.
 *
 * This asynchronous operation runs forever unless it is cancelled, the rpc handler throws an exception or the server is
 * shutdown
 * ([grpc::Server::Shutdown](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_interface.html#a6a1d337270116c95f387e0abf01f6c6c)
 * is called). At which point it invokes the receiver (passing forward the exception thrown by the request handler, if
 * any) after all sender produced by invoking the rpc handler complete.
 *
 * Example:
 *
 * @snippet unifex_server.cpp server-rpc-unary-sender
 *
 * @tparam ServerRPC An instantiation of `agrpc::ServerRPC`
 * @param grpc_context The GrpcContext used to handle each rpc
 * @param service The service associated with the gRPC method of the ServerRPC
 * @param rpc_handler A callable that produces a sender
 *
 * @since 2.7.0
 */
template <class ServerRPC, class RPCHandler>
[[nodiscard]] detail::RPCHandlerSender<ServerRPC, RPCHandler> register_sender_rpc_handler(
    agrpc::GrpcContext& grpc_context, detail::GetServerRPCServiceT<ServerRPC>& service, RPCHandler rpc_handler)
{
    return {grpc_context, service, static_cast<RPCHandler&&>(rpc_handler)};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REGISTER_SENDER_RPC_HANDLER_HPP
