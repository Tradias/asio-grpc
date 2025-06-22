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

#ifndef AGRPC_AGRPC_REGISTER_CALLBACK_RPC_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_CALLBACK_RPC_HANDLER_HPP

#include <agrpc/detail/config.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

#include <agrpc/detail/register_callback_rpc_handler.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Register a callback rpc handler for the given method
 *
 * The rpc handler will be invoked for every incoming request of this gRPC method. It must take `ServerRPC::Ptr` as
 * first and `ServerRPC::Request&` as second (only for unary and server-streaming rpcs) argument. The ServerRPC is
 * automatically cancelled during destruction of the `ServerRPC::Ptr` if `finish()` was not called earlier.
 *
 * This asynchronous operation runs forever unless it is cancelled, the rpc handler throws an exception or the server is
 * shutdown
 * ([grpc::Server::Shutdown](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_interface.html#a6a1d337270116c95f387e0abf01f6c6c)
 * is called). At which point it invokes the completion handler (passing forward the exception thrown by the request
 * handler, if any) after all `ServerRPC::Ptr`s have been destructed.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-unary-callback
 *
 * [(experimental) Additionally, the rpc handler may have a method called `request_message_factory()`. If it does then
 * that method will be invoked and the returned object used to create and destroy the initial request message for unary
 * and server-streaming rpcs.
 *
 * Example: (since 3.4.0)]
 *
 * @snippet server_rpc.cpp server-rpc-handler-with-arena
 *
 * @tparam ServerRPC An instantiation of `agrpc::ServerRPC`
 * @param executor The executor used to handle each rpc
 * @param service The service associated with the gRPC method of the ServerRPC
 * @param rpc_handler A callable that handles the client's request
 * @param token A completion token for signature `void(std::exception_ptr)`.
 *
 * @since 2.8.0
 */
template <class ServerRPC, class RPCHandler,
          class CompletionToken = detail::DefaultCompletionTokenT<typename ServerRPC::executor_type>>
auto register_callback_rpc_handler(const typename ServerRPC::executor_type& executor,
                                   detail::ServerRPCServiceT<ServerRPC>& service, RPCHandler rpc_handler,
                                   CompletionToken&& token = CompletionToken{})
{
    using RequestMessageFactory = detail::ServerRPCPtrRequestMessageFactoryT<ServerRPC, RPCHandler>;
    using CheckRPCHandlerTakesServerRPCPtrAsArg [[maybe_unused]] =
        detail::RPCHandlerInvokeResultT<typename ServerRPC::Ptr, RPCHandler&, RequestMessageFactory&>;
    return asio::async_initiate<CompletionToken, void(std::exception_ptr)>(
        detail::RegisterCallbackRPCHandlerInitiator<ServerRPC>{service}, token, executor,
        static_cast<RPCHandler&&>(rpc_handler));
}

/**
 * @brief Register a callback rpc handler for the given method (GrpcContext overload)
 *
 * @since 2.8.0
 */
template <class ServerRPC, class RPCHandler, class CompletionToken>
auto register_callback_rpc_handler(agrpc::GrpcContext& grpc_context, detail::ServerRPCServiceT<ServerRPC>& service,
                                   RPCHandler&& rpc_handler, CompletionToken&& token)
{
    return agrpc::register_callback_rpc_handler<ServerRPC>(grpc_context.get_executor(), service,
                                                           static_cast<RPCHandler&&>(rpc_handler),
                                                           static_cast<CompletionToken&&>(token));
}

AGRPC_NAMESPACE_END

#endif

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_REGISTER_CALLBACK_RPC_HANDLER_HPP
