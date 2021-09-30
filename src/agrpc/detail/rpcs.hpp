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

#ifndef AGRPC_DETAIL_RPCS_HPP
#define AGRPC_DETAIL_RPCS_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/initiate.hpp"

#include <grpcpp/alarm.h>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

#if (BOOST_VERSION >= 107700)
#include <boost/asio/associated_cancellation_slot.hpp>
#endif

#include <utility>

namespace agrpc::detail
{
template <class RPC, class Request, class Responder>
using ServerMultiArgRequest = void (RPC::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                            grpc::ServerCompletionQueue*, void*);

template <class RPC, class Responder>
using ServerSingleArgRequest = void (RPC::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                             grpc::ServerCompletionQueue*, void*);

template <class RPC, class Request, class Reader>
using ClientUnaryRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*);

template <class RPC, class Request, class Reader>
using ClientServerStreamingRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*,
                                                     void*);

template <class RPC, class Writer, class Response>
using ClientSideStreamingRequest = Writer (RPC::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*, void*);

template <class RPC, class ReaderWriter>
using ClientBidirectionalStreamingRequest = ReaderWriter (RPC::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

#if (BOOST_VERSION >= 107700)
struct AlarmCancellationHandler
{
    grpc::Alarm& alarm;

    constexpr explicit AlarmCancellationHandler(grpc::Alarm& alarm) noexcept : alarm(alarm) {}

    void operator()(asio::cancellation_type type)
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            alarm.Cancel();
        }
    }
};
#endif
}  // namespace agrpc::detail

namespace agrpc
{
template <class RPC, class Service, class Request, class Responder,
          class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
             grpc::ServerContext& server_context, Request& request, Responder& responder, CompletionToken token = {});

template <class RPC, class Service, class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, grpc::ServerContext& server_context,
             Responder& responder, CompletionToken token = {});

template <class RPCContextImplementationAllocator>
class RPCRequestContext;

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
        return std::invoke(std::forward<Handler>(handler), this->context, this->request, std::move(this->responder),
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
void repeatedly_request(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service, Handler handler)
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(handler);
    auto rpc_handler = detail::allocate<detail::MultiArgRPCContext<Request, Responder>>(allocator);
    auto& context = rpc_handler->context;
    auto& request = rpc_handler->request;
    auto& responder = rpc_handler->responder;
    agrpc::request(rpc, service, context, request, responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_handler), std::move(handler)});
}

template <class RPC, class Service, class Responder, class Handler>
void repeatedly_request(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, Handler handler)
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(handler);
    auto rpc_handler = detail::allocate<detail::SingleArgRPCContext<Responder>>(allocator);
    auto& context = rpc_handler->context;
    auto& responder = rpc_handler->responder;
    agrpc::request(rpc, service, context, responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_handler), std::move(handler)});
}

template <class RPC, class Service, class RPCHandler, class Handler>
void RequestRepeater<RPC, Service, RPCHandler, Handler>::operator()(bool ok)
{
    if (ok)
    {
        auto next_handler{this->handler};
        detail::repeatedly_request(this->rpc, this->service, std::move(next_handler));
    }
    std::move(this->handler)(detail::RPCContextImplementation::create(std::move(this->rpc_handler)), ok);
}
}  // namespace detail
}  // namespace agrpc

#endif  // AGRPC_DETAIL_RPCS_HPP
