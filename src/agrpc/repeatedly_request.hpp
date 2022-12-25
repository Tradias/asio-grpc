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

#ifndef AGRPC_AGRPC_REPEATEDLY_REQUEST_HPP
#define AGRPC_AGRPC_REPEATEDLY_REQUEST_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/namespace_cpp20.hpp>
#include <agrpc/detail/repeatedly_request_sender.hpp>
#include <agrpc/detail/rpc.hpp>
#include <agrpc/detail/rpc_context.hpp>
#include <agrpc/detail/use_sender.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/repeatedly_request_context.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/repeatedly_request.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{

AGRPC_NAMESPACE_CPP20_BEGIN()

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
 * request (if any) and the responder that were used when requesting the RPC. It must be kept alive until the RPC is
 * finished. The RequestHandler's associated allocator (or the queried allocator from its associated executor [until
 * v2.0.0]) will be used for the allocations needed for each request.
 *
 * `agrpc::repeatedly_request` will complete when it was cancelled, the `agrpc::GrpcContext` was stopped or the
 * `grpc::Server` been shutdown. It will **not** wait until all outstanding RPCs that are being processed by the
 * RequestHandler have completed.
 *
 * When using the special CompletionToken created by `agrpc::use_sender` the RequestHandler's signature must be:<br>
 * `sender auto operator()(grpc::ServerContext&, Request&, Responder&)` for unary and server-streaming requests and<br>
 * `sender auto operator()(grpc::ServerContext&, Responder&)` otherwise.<br>
 * A copy of the RequestHandler will be made for each request to avoid lifetime surprises.
 * For libunifex this is the only available overload of this function.
 *
 * @snippet unifex_server.cpp repeatedly-request-sender
 *
 * Another special overload of `agrpc::repeatedly_request` can be used by passing a RequestHandler with the following
 * signature:<br>
 * `awaitable auto operator()(grpc::ServerContext&, Request&, Responder&)` for unary and server-streaming requests
 * and<br>
 * `awaitable auto operator()(grpc::ServerContext&, Responder&)` otherwise.<br>
 * A copy of the RequestHandler will be made for each request to avoid lifetime surprises.
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
 *
 * **Per-Operation Cancellation**
 *
 * All. Upon cancellation, the operation completes after receiving the next request from the client. The next request
 * will still be handled normally.
 */
class RepeatedlyRequestFn
{
  private:
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class RPC, class RequestHandler, class CompletionToken>
    static auto impl(RPC rpc, detail::GetServiceT<RPC>& service, RequestHandler&& request_handler,
                     CompletionToken token)
    {
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
        using RPCContext = detail::RPCContextForRPCT<RPC>;
        if constexpr (detail::INVOKE_RESULT_IS_CO_SPAWNABLE<detail::RemoveCrefT<RequestHandler>&,
                                                            typename RPCContext::Signature>)
        {
            return asio::async_initiate<CompletionToken, void()>(detail::RepeatedlyRequestCoroutineInitiator{}, token,
                                                                 static_cast<RequestHandler&&>(request_handler), rpc,
                                                                 service);
        }
        else
#endif
        {
            return asio::async_initiate<CompletionToken, void()>(detail::RepeatedlyRequestInitiator{}, token,
                                                                 static_cast<RequestHandler&&>(request_handler), rpc,
                                                                 service);
        }
    }
#endif

    template <class RPC, class RequestHandler>
    static detail::RepeatedlyRequestSender<RPC, detail::RemoveCrefT<RequestHandler>> impl(
        RPC rpc, detail::GetServiceT<RPC>& service, RequestHandler&& request_handler, detail::UseSender token)
    {
        return {token.grpc_context, rpc, service, static_cast<RequestHandler&&>(request_handler)};
    }

  public:
    /**
     * @brief Overload for typed RPCs
     */
    template <class RPC, class RequestHandler, class CompletionToken = detail::NoOp>
    auto operator()(RPC rpc, detail::GetServiceT<RPC>& service, RequestHandler&& request_handler,
                    CompletionToken&& token = {}) const
    {
        return RepeatedlyRequestFn::impl(rpc, service, static_cast<RequestHandler&&>(request_handler),
                                         static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Overload for generic RPCs
     */
    template <class RequestHandler, class CompletionToken = detail::NoOp>
    auto operator()(grpc::AsyncGenericService& service, RequestHandler&& request_handler,
                    CompletionToken&& token = {}) const
    {
        return RepeatedlyRequestFn::impl(detail::GenericRPCMarker{}, service,
                                         static_cast<RequestHandler&&>(request_handler),
                                         static_cast<CompletionToken&&>(token));
    }
};

AGRPC_NAMESPACE_CPP20_END

}

/**
 * @brief Register a request handler for a RPC
 *
 * @link detail::RepeatedlyRequestFn
 * Server-side function to register request handlers.
 * @endlink
 */
inline constexpr detail::RepeatedlyRequestFn repeatedly_request{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLY_REQUEST_HPP
