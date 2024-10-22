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

#ifndef AGRPC_AGRPC_REGISTER_COROUTINE_RPC_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_COROUTINE_RPC_HANDLER_HPP

#include <agrpc/detail/awaitable.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/register_coroutine_rpc_handler.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Register a coroutine rpc handler for the given method
 *
 * @tparam ServerRPC An instantiation of `agrpc::ServerRPC`
 * @param executor The executor used to handle each rpc
 * @param service The service associated with the gRPC method of the ServerRPC
 * @param rpc_handler A callable that produces a coroutine. The coroutine's return value is ignored.
 * @param token A completion token for signature `void(std::exception_ptr)`.
 *
 * @since 3.3.0
 */
template <class ServerRPC, class CoroutineTraits, class RPCHandler,
          class CompletionToken = detail::DefaultCompletionTokenT<typename ServerRPC::executor_type>>
auto register_coroutine_rpc_handler(const typename ServerRPC::executor_type& executor,
                                    detail::ServerRPCServiceT<ServerRPC>& service, RPCHandler rpc_handler,
                                    CompletionToken&& token = CompletionToken{})
{
    return asio::async_initiate<CompletionToken, void(std::exception_ptr)>(
        detail::RegisterCoroutineRPCHandlerInitiator<ServerRPC, CoroutineTraits>{service}, token, executor,
        static_cast<RPCHandler&&>(rpc_handler));
}

/**
 * @brief (experimental) Register an coroutine rpc handler for the given method (GrpcContext overload)
 *
 * @since 3.3.0
 */
template <class ServerRPC, class CoroutineTraits, class RPCHandler, class CompletionToken>
auto register_coroutine_rpc_handler(agrpc::GrpcContext& grpc_context, detail::ServerRPCServiceT<ServerRPC>& service,
                                    RPCHandler&& rpc_handler, CompletionToken&& token)
{
    return agrpc::register_coroutine_rpc_handler<ServerRPC, CoroutineTraits>(grpc_context.get_executor(), service,
                                                                             static_cast<RPCHandler&&>(rpc_handler),
                                                                             static_cast<CompletionToken&&>(token));
}

AGRPC_NAMESPACE_END

#endif

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_REGISTER_COROUTINE_RPC_HANDLER_HPP
