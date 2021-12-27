// Copyright 2021 Dennis Hezel
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
#include "agrpc/initiate.hpp"
#include "agrpc/rpcs.hpp"

#include <tuple>

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

struct RPCContextBase
{
    grpc::ServerContext context{};

    constexpr auto& server_context() noexcept { return context; }
};

template <class Request, class Responder>
struct MultiArgRPCContext : detail::RPCContextBase
{
    Responder responder_{&this->context};
    Request request_{};

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->request_, this->responder_,
                           std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->request_, this->responder_); }

    constexpr auto& request() noexcept { return this->request_; }

    constexpr auto& responder() noexcept { return this->responder_; }
};

template <class Responder>
struct SingleArgRPCContext : detail::RPCContextBase
{
    Responder responder_{&this->context};

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->responder_,
                           std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->responder_); }

    constexpr auto& responder() noexcept { return this->responder_; }
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

template <class Allocator>
struct NoOpReceiverWithAllocator
{
    using allocator_type = Allocator;

    Allocator allocator;

    constexpr explicit NoOpReceiverWithAllocator(Allocator allocator) noexcept(
        std::is_nothrow_copy_constructible_v<Allocator>)
        : allocator(allocator)
    {
    }

    static constexpr void set_done() noexcept {}

    template <class... Args>
    static constexpr void set_value(Args&&...) noexcept
    {
    }

    static void set_error(std::exception_ptr) noexcept {}

    constexpr auto get_allocator() const noexcept { return allocator; }

#ifdef AGRPC_UNIFEX
    friend constexpr auto tag_invoke(unifex::tag_t<unifex::get_allocator>,
                                     const NoOpReceiverWithAllocator& receiver) noexcept
    {
        return receiver.allocator;
    }
#endif
};

template <class RPC, class Service, class Handler>
void submit_request_repeat(RPC rpc, Service& service, Handler handler, detail::UseSender use_sender)
{
    const auto allocator = detail::get_allocator(handler);
    detail::submit(agrpc::repeatedly_request(rpc, service, std::move(handler), use_sender),
                   detail::NoOpReceiverWithAllocator{allocator});
}

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
#ifdef AGRPC_UNIFEX
    return unifex::let_value_with(
        [&]
        {
            return detail::MultiArgRPCContext<Request, Responder>();
        },
        [&service, rpc, use_sender, sender_factory = std::move(sender_factory)](auto& context) mutable
        {
            return unifex::let_value(
                agrpc::request(rpc, service, context.server_context(), context.request(), context.responder(),
                               use_sender),
                [&context, &service, rpc, use_sender, sender_factory = std::move(sender_factory)](bool ok) mutable
                {
                    if AGRPC_LIKELY (ok)
                    {
                        detail::submit_request_repeat(rpc, service, sender_factory, use_sender);
                    }
                    return std::move(sender_factory)(context.server_context(), context.request(), context.responder());
                });
        });
#endif
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
#ifdef AGRPC_UNIFEX
    return unifex::let_value_with(
        [&]
        {
            return detail::SingleArgRPCContext<Responder>();
        },
        [&service, rpc, use_sender, sender_factory = std::move(sender_factory)](auto& context) mutable
        {
            return unifex::let_value(
                agrpc::request(rpc, service, context.server_context(), context.responder(), use_sender),
                [&context, &service, rpc, use_sender, sender_factory = std::move(sender_factory)](bool ok) mutable
                {
                    if AGRPC_LIKELY (ok)
                    {
                        detail::submit_request_repeat(rpc, service, sender_factory, use_sender);
                    }
                    return std::move(sender_factory)(context.server_context(), context.responder());
                });
        });
#endif
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
