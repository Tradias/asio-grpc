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

#ifndef AGRPC_AGRPC_REGISTER_AWAITABLE_RPC_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_AWAITABLE_RPC_HANDLER_HPP

#include <agrpc/detail/asio_forward.hpp>

#include <agrpc/detail/awaitable.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

#include <agrpc/detail/register_awaitable_rpc_handler.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Register an awaitable rpc handler for the given method
 *
 * The rpc handler will be invoked for every incoming request of this gRPC method. It must take `ServerRPC&` as
 * first argument and `ServerRPC::Request&` as second argument (only for unary and server-streaming rpcs). The ServerRPC
 * is automatically cancelled at the end of the rpc handler if `finish()` was not called earlier. The return value of
 * the rpc handler is `co_spawn`ed in a manner similar to:
 * `asio::co_spawn(asio::get_associated_executor(completion_handler, executor), rpc_handler())`, where
 * `completion_handler` is created from `token` and `executor` the first argument passed to this function.
 *
 * This asynchronous operation runs forever unless it is cancelled, the rpc handler throws an exception or the server is
 * shutdown
 * ([grpc::Server::Shutdown](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_interface.html#a6a1d337270116c95f387e0abf01f6c6c)
 * is called). At which point it invokes the completion handler (passing forward the exception thrown by the request
 * handler, if any) after all awaitables produced by invoking the rpc handler complete.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-unary
 *
 * @tparam ServerRPC An instantiation of `agrpc::ServerRPC`
 * @param executor The executor used to handle each rpc
 * @param service The service associated with the gRPC method of the ServerRPC
 * @param rpc_handler A callable that produces an `asio::awaitable<void, Executor>`. The awaitable's return
 * value is ignored. The Executor must be constructible from `asio::get_associated_executor(completion_handler,
 * executor)`, where `completion_handler` is obtained from `token` and `executor` the first argument passed to this
 * function.
 * @param token A completion token for signature `void(std::exception_ptr)`.
 *
 * @since 2.7.0
 */
template <class ServerRPC, class RPCHandler,
          class CompletionToken = detail::DefaultCompletionTokenT<typename ServerRPC::executor_type>>
auto register_awaitable_rpc_handler(const typename ServerRPC::executor_type& executor,
                                    detail::ServerRPCServiceT<ServerRPC>& service, RPCHandler rpc_handler,
                                    CompletionToken&& token = CompletionToken{})
{
    using Starter = detail::ServerRPCStarterT<ServerRPC>;
    static_assert(
        sizeof(detail::CoroutineTraits<detail::RPCHandlerInvokeResultT<Starter&, RPCHandler&, ServerRPC&>>) > 0,
        "Rpc handler must return an asio::awaitable and take ServerRPC& and, for server-streaming and unary rpcs, "
        "Request& as arguments.");
    return asio::async_initiate<CompletionToken, void(std::exception_ptr)>(
        detail::RegisterAwaitableRPCHandlerInitiator<ServerRPC>{service}, token, executor,
        static_cast<RPCHandler&&>(rpc_handler));
}

/**
 * @brief Register an awaitable rpc handler for the given method (GrpcContext overload)
 *
 * @since 2.7.0
 */
template <class ServerRPC, class RPCHandler, class CompletionToken>
auto register_awaitable_rpc_handler(agrpc::GrpcContext& grpc_context, detail::ServerRPCServiceT<ServerRPC>& service,
                                    RPCHandler&& rpc_handler, CompletionToken&& token)
{
    return agrpc::register_awaitable_rpc_handler<ServerRPC>(grpc_context.get_executor(), service,
                                                            static_cast<RPCHandler&&>(rpc_handler),
                                                            static_cast<CompletionToken&&>(token));
}

AGRPC_NAMESPACE_END

#endif

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_REGISTER_AWAITABLE_RPC_HANDLER_HPP
