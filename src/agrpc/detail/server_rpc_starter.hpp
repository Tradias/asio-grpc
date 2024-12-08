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

template <class Request, bool HasInitialRequest, class... PrependedArgs>
struct ServerRPCStarter
{
    template <auto RequestRPC, class TraitsT, class Executor, class Service, class CompletionToken>
    auto start(agrpc::ServerRPC<RequestRPC, TraitsT, Executor>& rpc, Service& service, CompletionToken&& token)
    {
        using Responder = std::remove_reference_t<decltype(ServerRPCContextBaseAccess::responder(rpc))>;
        return detail::async_initiate_sender_implementation(
            RPCExecutorBaseAccess::grpc_context(rpc),
            detail::ServerRequestSenderInitiation<RequestRPC>{service, *request_},
            detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc},
            static_cast<CompletionToken&&>(token));
    }

    template <class RPCHandler, class RPC, class... AppendedArgs>
    decltype(auto) invoke(RPCHandler&& handler, PrependedArgs&&... prepend, RPC&& rpc, AppendedArgs&&... append)
    {
        return static_cast<RPCHandler&&>(handler)(static_cast<PrependedArgs&&>(prepend)..., static_cast<RPC&&>(rpc),
                                                  *request_, static_cast<AppendedArgs&&>(append)...);
    }

    Request* request_;
};

template <class Request, class... PrependedArgs>
struct ServerRPCStarter<Request, false, PrependedArgs...>
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

    template <class RPCHandler, class RPC, class... AppendedArgs>
    decltype(auto) invoke(RPCHandler&& handler, PrependedArgs&&... prepend, RPC&& rpc, AppendedArgs&&... append)
    {
        return static_cast<RPCHandler&&>(handler)(static_cast<PrependedArgs&&>(prepend)..., static_cast<RPC&&>(rpc),
                                                  static_cast<AppendedArgs&&>(append)...);
    }
};

template <class ServerRPC, class... PrependedArgs>
using ServerRPCStarterT = detail::ServerRPCStarter<typename ServerRPC::Request,
                                                   detail::has_initial_request(ServerRPC::TYPE), PrependedArgs...>;

template <class Request>
struct DefaultRequestMessageFactory
{
    template <class>
    Request& create()
    {
        return request_;
    }

    Request request_;
};

template <class RequestT, class RPCHandler, class = void>
struct RequestMessageFactoryBuilder
{
    static constexpr bool IS_DEFAULT = true;

    using Request = RequestT;
    using Type = DefaultRequestMessageFactory<RequestT>;

    static Type build(RPCHandler&) { return Type{}; }
};

template <class RequestT, class RPCHandler>
struct RequestMessageFactoryBuilder<RequestT, RPCHandler,
                                    decltype((void)std::declval<RPCHandler&>().request_message_factory())>
{
    static constexpr bool IS_DEFAULT = false;

    using Request = RequestT;
    using Type = decltype(std::declval<RPCHandler&>().request_message_factory());

    static Type build(RPCHandler& rpc_handler) { return rpc_handler.request_message_factory(); }
};

template <class Request, class RequestMessageFactory, class = void>
inline constexpr bool REQUEST_MESSAGE_FACTORY_HAS_DESTROY = false;

template <class Request, class RequestMessageFactory>
inline constexpr bool REQUEST_MESSAGE_FACTORY_HAS_DESTROY<Request, RequestMessageFactory,
                                                          decltype((void)std::declval<RequestMessageFactory&>().destroy(
                                                              std::declval<Request&>()))> = true;

template <class Base, class RequestMessageFactoryBuilder, bool HasInitialRequest>
struct RequestMessageFactoryMixin : Base
{
    using RequestMessageFactory = typename RequestMessageFactoryBuilder::Type;
    using Request = typename RequestMessageFactoryBuilder::Request;

    template <class RPCHandler, class... Args>
    explicit RequestMessageFactoryMixin(RPCHandler& rpc_handler, Args&&... args)
        : Base{static_cast<Args&&>(args)...}, request_factory_(RequestMessageFactoryBuilder::build(rpc_handler))
    {
        this->request_ = &request_factory_.template create<Request>();
    }

    RequestMessageFactoryMixin(const RequestMessageFactoryMixin& other) = delete;
    RequestMessageFactoryMixin(RequestMessageFactoryMixin&& other) = delete;

    ~RequestMessageFactoryMixin()
    {
        if constexpr (REQUEST_MESSAGE_FACTORY_HAS_DESTROY<Request, RequestMessageFactory>)
        {
            static_assert(noexcept(request_factory_.destroy(*this->request_)),
                          "Request message factory `destroy(Request&)` must be noexcept");
            request_factory_.destroy(*this->request_);
        }
    }

    RequestMessageFactoryMixin& operator=(const RequestMessageFactoryMixin& other) = delete;
    RequestMessageFactoryMixin& operator=(RequestMessageFactoryMixin&& other) = delete;

    template <class... Args>
    decltype(auto) invoke(Args&&... args)
    {
        if constexpr (RequestMessageFactoryBuilder::IS_DEFAULT)
        {
            return Base::invoke(static_cast<Args&&>(args)...);
        }
        else
        {
            return Base::invoke(static_cast<Args&&>(args)..., request_factory_);
        }
    }

    RequestMessageFactory request_factory_;
};

template <class Base, class RequestMessageFactoryBuilder>
struct RequestMessageFactoryMixin<Base, RequestMessageFactoryBuilder, false> : Base
{
    template <class RPCHandler, class... Args>
    explicit RequestMessageFactoryMixin(RPCHandler&, Args&&... args) : Base{static_cast<Args&&>(args)...}
    {
    }

    RequestMessageFactoryMixin(const RequestMessageFactoryMixin& other) = delete;
    RequestMessageFactoryMixin(RequestMessageFactoryMixin&& other) = delete;
    RequestMessageFactoryMixin& operator=(const RequestMessageFactoryMixin& other) = delete;
    RequestMessageFactoryMixin& operator=(RequestMessageFactoryMixin&& other) = delete;
};

template <class Base, class ServerRPC, class RPCHandler>
using RequestMessageFactoryServerRPCMixinT =
    detail::RequestMessageFactoryMixin<Base,
                                       detail::RequestMessageFactoryBuilder<typename ServerRPC::Request, RPCHandler>,
                                       detail::has_initial_request(ServerRPC::TYPE)>;

template <class ServerRPC, class RPCHandler, class... Args>
using RequestMessageFactoryServerRPCStarter =
    detail::RequestMessageFactoryServerRPCMixinT<detail::ServerRPCStarterT<ServerRPC, Args...>, ServerRPC, RPCHandler>;

template <class Starter, class Handler, class RPC, class... Args>
using RPCHandlerInvokeResultT =
    decltype(std::declval<Starter>().invoke(std::declval<Handler>(), std::declval<RPC>(), std::declval<Args>()...));
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_STARTER_HPP
