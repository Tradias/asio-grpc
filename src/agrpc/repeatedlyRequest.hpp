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

#ifndef AGRPC_AGRPC_REPEATEDLYREQUEST_HPP
#define AGRPC_AGRPC_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/repeatedlyRequest.hpp"
#include "agrpc/detail/repeatedlyRequestSender.hpp"
#include "agrpc/detail/rpcContext.hpp"
#include "agrpc/repeatedlyRequestContext.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Server-side function object to register request handlers
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * This function helps to ensure that there are enough outstanding calls to `request` to match incoming RPCs. It takes a
 * RPC, a Service, a RequestHandler and a CompletionToken. The RequestHandler determines what to do with a client
 * request, it could e.g. spawn a new coroutine to process it. It must also have an associated executor that refers to a
 * `agrpc::GrpcContext`. When the client makes a request the RequestHandler is invoked with a
 * `agrpc::RepeatedlyRequestContext` - a move-only type that provides a stable address to the `grpc::ServerContext`, the
 * request (if any) and the responder that were used when requesting the RPC. It should be kept alive until the RPC is
 * finished. The RequestHandler or its associated executor may also have an associated allocator to control the
 * allocation needed for each request.
 *
 * `agrpc::repeatedly_request` will complete when it was cancelled, the `agrpc::GrpcContext` was stopped or the
 * `grpc::Server` been shutdown. It will **not** wait until all outstanding RPCs that are being processed by the
 * RequestHandler have completed.
 *
 * When using the special CompletionToken created by `agrpc::use_sender` the RequestHandler's signature must be:<br>
 * `sender auto operator()(grpc::ServerContext&, Request&, Responder&)` for unary and server-streaming requests and<br>
 * `sender auto operator()(grpc::ServerContext&, Responder&)` otherwise.<br>
 * For libunifex this is the only available overload of this function.
 *
 * @snippet unifex-server.cpp repeatedly-request-sender
 *
 * Another special overload of `agrpc::repeatedly_request` can be used by passing a RequestHandler with the following
 * signature:<br>
 * `awaitable auto operator()(grpc::ServerContext&, Request&, Responder&)` for unary and server-streaming requests
 * and<br>
 * `awaitable auto operator()(grpc::ServerContext&, Responder&)` otherwise.
 *
 * @snippet server.cpp repeatedly-request-awaitable
 *
 * The following example shows how to implement a RequestHandler with a custom allocator for simple, high-performance
 * RPC processing:
 *
 * @snippet server.cpp repeatedly-request-callback
 *
 * @param request_handler Any exception thrown by the invocation of the request handler will be rethrown by
 * GrpcContext#run(). Except for the sender version, where the exception will be send to the receiver.
 * @param token The completion signature is `void()`. If the token has been created by `agrpc::use_sender` then the
 * request handler must return a sender.
 */
class RepeatedlyRequestFn
{
  private:
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class RPC, class Service, class RequestHandler, class CompletionToken>
    static auto impl(RPC rpc, Service& service, RequestHandler&& request_handler, CompletionToken token)
    {
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
        using RPCContext = detail::RPCContextForRPCT<RPC>;
        if constexpr (detail::INVOKE_RESULT_IS_CO_SPAWNABLE<std::decay_t<RequestHandler>&,
                                                            typename RPCContext::Signature>)
        {
            return asio::async_initiate<CompletionToken, void()>(detail::RepeatedlyRequestAwaitableInitiator{}, token,
                                                                 std::forward<RequestHandler>(request_handler), rpc,
                                                                 service);
        }
        else
#endif
        {
            return asio::async_initiate<CompletionToken, void()>(detail::RepeatedlyRequestInitiator{}, token,
                                                                 std::forward<RequestHandler>(request_handler), rpc,
                                                                 service);
        }
    }
#endif

    template <class RPC, class Service, class RequestHandler>
    static auto impl(RPC rpc, Service& service, RequestHandler&& request_handler, detail::UseSender token)
    {
        return detail::RepeatedlyRequestSender{token.grpc_context, rpc, service,
                                               std::forward<RequestHandler>(request_handler)};
    }

  public:
    /**
     * @brief Overload for unary and server-streaming RPCs
     */
    template <class RPC, class Service, class Request, class Responder, class RequestHandler,
              class CompletionToken = detail::NoOp>
    auto operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                    RequestHandler&& request_handler, CompletionToken&& token = {}) const
    {
        return RepeatedlyRequestFn::impl(rpc, service, std::forward<RequestHandler>(request_handler),
                                         std::forward<CompletionToken>(token));
    }

    /**
     * @brief Overload for client-streaming and bidirectional RPCs
     */
    template <class RPC, class Service, class Responder, class RequestHandler, class CompletionToken = detail::NoOp>
    auto operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                    RequestHandler&& request_handler, CompletionToken&& token = {}) const
    {
        return RepeatedlyRequestFn::impl(rpc, service, std::forward<RequestHandler>(request_handler),
                                         std::forward<CompletionToken>(token));
    }
};
}  // namespace detail

/**
 * @brief Register a request handler for a RPC
 *
 * @link detail::RepeatedlyRequestFn
 * Server-side function to register request handlers.
 * @endlink
 */
inline constexpr detail::RepeatedlyRequestFn repeatedly_request{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLYREQUEST_HPP
