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
class RepeatedlyRequestFn
{
  private:
    template <class RPC, class Service, class RequestHandler, class CompletionToken>
    static auto impl(RPC rpc, Service& service, RequestHandler&& request_handler, CompletionToken token)
    {
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
        using RPCContext = detail::RPCContextForRPCT<RPC>;
        if constexpr (detail::INVOKE_RESULT_IS_SENDER<std::decay_t<RequestHandler>&, typename RPCContext::Signature>)
        {
#endif
            return detail::RepeatedlyRequestSender{token.grpc_context, rpc, service,
                                                   std::forward<RequestHandler>(request_handler)};
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
        }
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
        else if constexpr (detail::INVOKE_RESULT_IS_CO_SPAWNABLE<std::decay_t<RequestHandler>&,
                                                                 typename RPCContext::Signature>)
        {
            return asio::async_initiate<CompletionToken, void()>(detail::RepeatedlyRequestAwaitableInitiator{}, token,
                                                                 std::forward<RequestHandler>(request_handler), rpc,
                                                                 service);
        }
#endif
        else
        {
            return asio::async_initiate<CompletionToken, void()>(detail::RepeatedlyRequestInitiator{}, token,
                                                                 std::forward<RequestHandler>(request_handler), rpc,
                                                                 service);
        }
#endif
    }

  public:
    template <class RPC, class Service, class Request, class Responder, class RequestHandler,
              class CompletionToken = detail::NoOp>
    auto operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                    RequestHandler&& request_handler, CompletionToken&& token = {}) const
    {
        return RepeatedlyRequestFn::impl(rpc, service, std::forward<RequestHandler>(request_handler),
                                         std::forward<CompletionToken>(token));
    }

    template <class RPC, class Service, class Responder, class RequestHandler, class CompletionToken = detail::NoOp>
    auto operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                    RequestHandler&& request_handler, CompletionToken&& token = {}) const
    {
        return RepeatedlyRequestFn::impl(rpc, service, std::forward<RequestHandler>(request_handler),
                                         std::forward<CompletionToken>(token));
    }
};
}  // namespace detail

inline constexpr detail::RepeatedlyRequestFn repeatedly_request{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLYREQUEST_HPP
