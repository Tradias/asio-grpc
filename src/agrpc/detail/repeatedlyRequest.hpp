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
struct RPCContextImplementation
{
    template <class RPCContextImplementationAllocator>
    static constexpr auto create(detail::AllocatedPointer<RPCContextImplementationAllocator>&& impl) noexcept
    {
        return agrpc::RPCRequestContext{std::move(impl)};
    }
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct RPCContextBase
{
    grpc::ServerContext context{};
};

template <class Request, class Responder>
struct MultiArgRPCContext : detail::RPCContextBase
{
    Responder responder{&this->context};
    Request request{};

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->request, this->responder,
                           std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->request, this->responder); }
};

template <class Responder>
struct SingleArgRPCContext : detail::RPCContextBase
{
    Responder responder{&this->context};

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->responder, std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->responder); }
};

template <class RPC, class Service, class RPCHandlerAllocator, class Handler>
struct RequestRepeater
{
    using executor_type = asio::associated_executor_t<Handler>;
    using allocator_type = asio::associated_allocator_t<Handler>;

    RPC rpc;
    Service& service;
    detail::AllocatedPointer<RPCHandlerAllocator> rpc_handler;
    Handler handler;

    RequestRepeater(RPC rpc, Service& service, detail::AllocatedPointer<RPCHandlerAllocator> rpc_handler,
                    Handler handler)
        : rpc(rpc), service(service), rpc_handler(std::move(rpc_handler)), handler(std::move(handler))
    {
    }

    void operator()(bool ok);

    executor_type get_executor() const noexcept { return asio::get_associated_executor(handler); }

    allocator_type get_allocator() const noexcept { return asio::get_associated_allocator(handler); }
};

template <class RPC, class Service, class Request, class Responder, class Handler>
void RepeatedlyRequestFn::operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                     Handler handler) const
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(handler);
    auto rpc_handler = detail::allocate<detail::MultiArgRPCContext<Request, Responder>>(allocator);
    auto& rpc_context = rpc_handler->context;
    auto& rpc_request = rpc_handler->request;
    auto& rpc_responder = rpc_handler->responder;
    agrpc::request(rpc, service, rpc_context, rpc_request, rpc_responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_handler), std::move(handler)});
}

template <class RPC, class Service, class Responder, class Handler>
void RepeatedlyRequestFn::operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                     Handler handler) const
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(handler);
    auto rpc_handler = detail::allocate<detail::SingleArgRPCContext<Responder>>(allocator);
    auto& rpc_context = rpc_handler->context;
    auto& rpc_responder = rpc_handler->responder;
    agrpc::request(rpc, service, rpc_context, rpc_responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_handler), std::move(handler)});
}

template <class RPC, class Service, class RPCHandler, class Handler>
void RequestRepeater<RPC, Service, RPCHandler, Handler>::operator()(bool ok)
{
    if (ok) AGRPC_LIKELY
        {
            auto next_handler{this->handler};
            agrpc::repeatedly_request(this->rpc, this->service, std::move(next_handler));
        }
    std::move(this->handler)(detail::RPCContextImplementation::create(std::move(this->rpc_handler)), ok);
}
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
