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

#ifndef AGRPC_DETAIL_SERVER_RPC_STARTER_HPP
#define AGRPC_DETAIL_SERVER_RPC_STARTER_HPP

#include <agrpc/detail/server_rpc_request_message.hpp>
#include <agrpc/rpc_type.hpp>
#include <agrpc/server_rpc.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Request, class RequestMessageFactory, class = void>
inline constexpr bool REQUEST_MESSAGE_FACTORY_HAS_DESTROY = false;

template <class Request, class RequestMessageFactory>
inline constexpr bool REQUEST_MESSAGE_FACTORY_HAS_DESTROY<Request, RequestMessageFactory,
                                                          decltype((void)std::declval<RequestMessageFactory&>().destroy(
                                                              std::declval<Request&>()))> = true;

using DefaultRequestMessageFactory = void;

template <class RPCHandler, class = void>
struct GetRPCHandlerRequestMessageFactory
{
    using Type = DefaultRequestMessageFactory;
};

template <class RPCHandler>
struct GetRPCHandlerRequestMessageFactory<RPCHandler,
                                          decltype((void)std::declval<RPCHandler&>().request_message_factory())>
{
    using Type = decltype(std::declval<RPCHandler&>().request_message_factory());
};

template <class RPCHandler>
using RPCHandlerRequestMessageFactoryT = typename GetRPCHandlerRequestMessageFactory<RPCHandler>::Type;

template <template <class, bool> class BaseT, class RequestT, class Factory>
struct RequestMessageFactoryBuilderMixin : BaseT<RequestT, true>
{
    static constexpr bool HAS_CUSTOM_FACTORY = true;

    using Base = BaseT<RequestT, true>;

    template <class RPCHandler, class... Args>
    explicit RequestMessageFactoryBuilderMixin(RPCHandler& rpc_handler, Args&&... args)
        : Base{static_cast<Args&&>(args)...}, request_factory_(rpc_handler.request_message_factory())
    {
        this->request_ = &request_factory_.template create<RequestT>();
    }

    ~RequestMessageFactoryBuilderMixin() noexcept
    {
        if constexpr (REQUEST_MESSAGE_FACTORY_HAS_DESTROY<RequestT, Factory>)
        {
            static_assert(noexcept(request_factory_.destroy(*this->request_)),
                          "Request message factory `destroy(Request&)` must be noexcept");
            request_factory_.destroy(*this->request_);
        }
    }

    RequestT& get_request() noexcept { return *this->request_; }

    Factory& get_factory() noexcept { return request_factory_; }

    Factory request_factory_;
};

template <template <class, bool> class BaseT, class RequestT>
struct RequestMessageFactoryBuilderMixin<BaseT, RequestT, DefaultRequestMessageFactory> : BaseT<RequestT, false>
{
    static constexpr bool HAS_CUSTOM_FACTORY = false;

    using Base = BaseT<RequestT, false>;

    template <class RPCHandler, class... Args>
    explicit RequestMessageFactoryBuilderMixin(RPCHandler&, Args&&... args) : Base{static_cast<Args&&>(args)...}
    {
        if constexpr (Base::HAS_REQUEST_PTR)
        {
            this->request_ = &request_message_;
        }
    }

    RequestT& get_request() noexcept { return request_message_; }

    RequestT request_message_;
};

template <template <class, bool> class BaseT, class RequestT, class Factory, bool HasInitialRequest>
struct RequestMessageFactoryMixin : RequestMessageFactoryBuilderMixin<BaseT, RequestT, Factory>
{
    static constexpr bool HAS_INITIAL_REQUEST = true;

    template <class RPCHandler, class... Args>
    explicit RequestMessageFactoryMixin(RPCHandler& rpc_handler, Args&&... args)
        : RequestMessageFactoryMixin::RequestMessageFactoryBuilderMixin(rpc_handler, static_cast<Args&&>(args)...)
    {
    }
};

template <template <class, bool> class BaseT, class RequestT, class Factory>
struct RequestMessageFactoryMixin<BaseT, RequestT, Factory, false> : BaseT<RequestT, false>
{
    static constexpr bool HAS_INITIAL_REQUEST = false;

    using Base = BaseT<RequestT, false>;

    template <class RPCHandler, class... Args>
    explicit RequestMessageFactoryMixin(RPCHandler&, Args&&... args) : Base{static_cast<Args&&>(args)...}
    {
    }
};

struct PickServerRPCRequestMessage
{
    template <class Request, bool NeedsRequestPtr>
    using Type = detail::ServerRPCRequestMessage<Request, NeedsRequestPtr>;
};

template <template <class, bool> class Base, class ServerRPC, class RPCHandler>
using RequestMessageFactoryServerRPCMixinT =
    detail::RequestMessageFactoryMixin<Base, typename ServerRPC::Request,
                                       detail::RPCHandlerRequestMessageFactoryT<RPCHandler>,
                                       detail::has_initial_request(ServerRPC::TYPE)>;

template <class ServerRPC, class RPCHandler>
using ServerRPCRequestMessageFactoryT =
    detail::RequestMessageFactoryServerRPCMixinT<PickServerRPCRequestMessage::template Type, ServerRPC, RPCHandler>;

template <class... PrependedArgs>
struct ServerRPCStarter
{
    template <auto RequestRPC, class TraitsT, class Executor, class Service, class RequestMessageFactory,
              class CompletionToken>
    static auto start(agrpc::ServerRPC<RequestRPC, TraitsT, Executor>& rpc, Service& service,
                      RequestMessageFactory& factory, CompletionToken&& token)
    {
        using Responder = std::remove_reference_t<decltype(ServerRPCContextBaseAccess::responder(rpc))>;
        if constexpr (RequestMessageFactory::HAS_INITIAL_REQUEST)
        {
            return detail::async_initiate_sender_implementation(
                RPCExecutorBaseAccess::grpc_context(rpc),
                detail::ServerRequestSenderInitiation<RequestRPC>{service, factory.get_request()},
                detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc},
                static_cast<CompletionToken&&>(token));
        }
        else
        {
            return detail::async_initiate_sender_implementation(
                RPCExecutorBaseAccess::grpc_context(rpc), detail::ServerRequestSenderInitiation<RequestRPC>{service},
                detail::ServerRequestSenderImplementation<Responder, TraitsT::NOTIFY_WHEN_DONE>{rpc},
                static_cast<CompletionToken&&>(token));
        }
    }

    template <class RPCHandler, class RPC, class RequestMessageFactory, class... AppendedArgs>
    static decltype(auto) invoke(RPCHandler&& handler, PrependedArgs&&... prepend, RPC&& rpc,
                                 RequestMessageFactory& factory, AppendedArgs&&... append)
    {
        if constexpr (RequestMessageFactory::HAS_INITIAL_REQUEST)
        {
            if constexpr (RequestMessageFactory::HAS_CUSTOM_FACTORY)
            {
                return static_cast<RPCHandler&&>(handler)(
                    static_cast<PrependedArgs&&>(prepend)..., static_cast<RPC&&>(rpc), factory.get_request(),
                    static_cast<AppendedArgs&&>(append)..., factory.get_factory());
            }
            else
            {
                return static_cast<RPCHandler&&>(handler)(static_cast<PrependedArgs&&>(prepend)...,
                                                          static_cast<RPC&&>(rpc), factory.get_request(),
                                                          static_cast<AppendedArgs&&>(append)...);
            }
        }
        else
        {
            return static_cast<RPCHandler&&>(handler)(static_cast<PrependedArgs&&>(prepend)..., static_cast<RPC&&>(rpc),
                                                      static_cast<AppendedArgs&&>(append)...);
        }
    }
};

template <class ServerRPC, class RPCHandler, class RequestMessageFactory, class... Args>
using RPCHandlerInvokeResultT =
    decltype(ServerRPCStarter<Args...>::invoke(std::declval<RPCHandler>(), std::declval<ServerRPC>(),
                                               std::declval<RequestMessageFactory>(), std::declval<Args>()...));
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_STARTER_HPP
