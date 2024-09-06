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

#ifndef AGRPC_DETAIL_SERVER_RPC_STARTER_HPP
#define AGRPC_DETAIL_SERVER_RPC_STARTER_HPP

#include <agrpc/rpc_type.hpp>
#include <agrpc/server_rpc.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
constexpr bool has_initial_request(agrpc::ServerRPCType type) noexcept
{
    return type == agrpc::ServerRPCType::SERVER_STREAMING || type == agrpc::ServerRPCType::UNARY;
}

template <class Request, bool HasInitialRequest>
struct ServerRPCStarter
{
    template <auto RequestRPC, class TraitsT, class Executor, class Service, class CompletionToken>
    auto start(agrpc::ServerRPC<RequestRPC, TraitsT, Executor>& rpc, Service& service, CompletionToken&& token)
    {
        using Responder = std::remove_reference_t<decltype(ServerRPCContextBaseAccess::responder(rpc))>;
        return detail::async_initiate_sender_implementation(
            RPCExecutorBaseAccess::grpc_context(rpc),
            detail::ServerRequestSenderInitiation<RequestRPC>{service, request_},
            detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc},
            static_cast<CompletionToken&&>(token));
    }

    template <class Handler, class RPC, class... Args>
    decltype(auto) invoke(Handler&& handler, RPC&& rpc, Args&&... args)
    {
        return static_cast<Handler&&>(handler)(static_cast<RPC&&>(rpc), request_, static_cast<Args&&>(args)...);
    }

    Request request_;
};

template <class Request>
struct ServerRPCStarter<Request, false>
{
    template <auto RequestRPC, class TraitsT, class Executor, class Service, class CompletionToken>
    auto start(agrpc::ServerRPC<RequestRPC, TraitsT, Executor>& rpc, Service& service, CompletionToken&& token)
    {
        using Responder = std::remove_reference_t<decltype(ServerRPCContextBaseAccess::responder(rpc))>;
        return detail::async_initiate_sender_implementation(
            RPCExecutorBaseAccess::grpc_context(rpc), detail::ServerRequestSenderInitiation<RequestRPC>{service},
            detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc},
            static_cast<CompletionToken&&>(token));
    }

    template <class Handler, class RPC, class... Args>
    decltype(auto) invoke(Handler&& handler, RPC&& rpc, Args&&... args)
    {
        return static_cast<Handler&&>(handler)(static_cast<RPC&&>(rpc), static_cast<Args&&>(args)...);
    }
};

template <class ServerRPC>
using ServerRPCStarterT =
    detail::ServerRPCStarter<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>;

template <class Starter, class Handler, class RPC, class... Args>
using RPCHandlerInvokeResultT =
    decltype(std::declval<Starter>().invoke(std::declval<Handler>(), std::declval<RPC>(), std::declval<Args>()...));
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_STARTER_HPP
