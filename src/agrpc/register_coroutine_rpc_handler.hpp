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
 * The rpc handler will be invoked for every incoming request of this gRPC method. It must take `ServerRPC&` as
 * first argument and `ServerRPC::Request&` as second argument (only for unary and server-streaming rpcs). The ServerRPC
 * is automatically cancelled at the end of the rpc handler if `finish()` was not called earlier. The return value of
 * the rpc handler is `co_spawn`ed in a manner similar to:
 * `CoroutineTraits::co_spawn(executor, rpc_handler, completion_handler, function)`, where
 * `completion_handler` is created from `token`, `executor` the first argument passed to this function and `function`,
 * when invoked, starts waiting for the next rpc. Any arguments passed to `function` will be prepended to the call of
 * the rpc handler. The return type of `function` is `CoroutineTraits::ReturnType`, which must be a coroutine, and
 * `CoroutineTraits::completion_token` must produce an Asio compatible [completion
 * token](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers)
 * that, when used to initiate an asynchronous operation, returns an awaitable.
 *
 * This asynchronous operation runs forever unless it is cancelled, the rpc handler throws an exception or the server is
 * shutdown
 * ([grpc::Server::Shutdown](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_interface.html#a6a1d337270116c95f387e0abf01f6c6c)
 * is called). At which point it invokes the completion handler (passing forward the exception thrown by the request
 * handler, if any) after all coroutines produced by invoking the rpc handler complete.
 *
 * @tparam ServerRPC An instantiation of `agrpc::ServerRPC`
 * @tparam CoroutineTraits A class that provides functions for spawning the coroutine of each rpc. Example:
 *
 * @snippet coro_traits.hpp asio-coro-traits
 *
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
