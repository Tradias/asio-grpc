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

#ifndef AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
#define AGRPC_DETAIL_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/repeatedlyRequestSender.hpp"
#include "agrpc/detail/rpcContext.hpp"
#include "agrpc/initiate.hpp"
#include "agrpc/rpcs.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RepeatedlyRequestContextAccess
{
    template <class ImplementationAllocator>
    static constexpr auto create(detail::AllocatedPointer<ImplementationAllocator>&& allocated_pointer) noexcept
    {
        return agrpc::RepeatedlyRequestContext{std::move(allocated_pointer)};
    }
};

template <class RPC, class Service, class RPCContextAllocator, class Handler>
struct RequestRepeater
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    using executor_type = asio::associated_executor_t<Handler>;
    using allocator_type = asio::associated_allocator_t<Handler>;
#endif

    RPC rpc;
    Service& service;
    detail::AllocatedPointer<RPCContextAllocator> rpc_context;
    Handler handler;

    RequestRepeater(RPC rpc, Service& service, detail::AllocatedPointer<RPCContextAllocator> rpc_context,
                    Handler handler)
        : rpc(rpc), service(service), rpc_context(std::move(rpc_context)), handler(std::move(handler))
    {
    }

    void operator()(bool ok);

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    executor_type get_executor() const noexcept { return asio::get_associated_executor(handler); }

    allocator_type get_allocator() const noexcept { return asio::get_associated_allocator(handler); }
#else
    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const RequestRepeater& self) noexcept
    {
        return detail::get_allocator(self.handler);
    }
#endif
};

template <class RPC, class Service, class Request, class Responder, class Handler>
void RepeatedlyRequestFn::operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                     Handler handler) const
{
    auto rpc_context = detail::allocate<detail::MultiArgRPCContext<Request, Responder>>(detail::get_allocator(handler));
    auto& rpc_server_context = rpc_context->server_context();
    auto& rpc_request = rpc_context->request();
    auto& rpc_responder = rpc_context->responder();
    agrpc::request(rpc, service, rpc_server_context, rpc_request, rpc_responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_context), std::move(handler)});
}

template <class RPC, class Service, class Request, class Responder, class SenderFactory>
auto RepeatedlyRequestFn::operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                     SenderFactory sender_factory, detail::UseSender use_sender) const
{
    return detail::RepeatedlyRequestSender{use_sender.grpc_context, rpc, service, std::move(sender_factory)};
}

template <class RPC, class Service, class Responder, class Handler>
void RepeatedlyRequestFn::operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                     Handler handler) const
{
    auto rpc_context = detail::allocate<detail::SingleArgRPCContext<Responder>>(detail::get_allocator(handler));
    auto& rpc_server_context = rpc_context->server_context();
    auto& rpc_responder = rpc_context->responder();
    agrpc::request(rpc, service, rpc_server_context, rpc_responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_context), std::move(handler)});
}

template <class RPC, class Service, class Responder, class SenderFactory>
auto RepeatedlyRequestFn::operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                     SenderFactory sender_factory, detail::UseSender use_sender) const
{
    return detail::RepeatedlyRequestSender{use_sender.grpc_context, rpc, service, std::move(sender_factory)};
}

template <class RPC, class Service, class RPCHandler, class Handler>
void RequestRepeater<RPC, Service, RPCHandler, Handler>::operator()(bool ok)
{
    if AGRPC_LIKELY (ok)
    {
        auto next_handler{this->handler};
        agrpc::repeatedly_request(this->rpc, this->service, std::move(next_handler));
    }
    std::move(this->handler)(detail::RepeatedlyRequestContextAccess::create(std::move(this->rpc_context)), ok);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
