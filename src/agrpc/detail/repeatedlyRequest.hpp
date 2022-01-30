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
#include "agrpc/detail/utility.hpp"
#include "agrpc/detail/workTrackingCompletionHandler.hpp"
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

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Handler, class Args, class = void>
inline constexpr bool IS_REPEATEDLY_REQUEST_SENDER_FACTORY = false;

template <class Handler, class... Args>
inline constexpr bool IS_REPEATEDLY_REQUEST_SENDER_FACTORY<
    Handler, void(Args...), std::enable_if_t<detail::is_sender_v<std::invoke_result_t<Handler&&, Args...>>>> = true;

template <class RPC, class Service, class RequestHandler>
class RepeatedlyRequestContext
{
  public:
    RepeatedlyRequestContext(RPC rpc, Service& service, RequestHandler request_handler)
        : impl1(rpc), impl2(service, std::move(request_handler))
    {
    }

    [[nodiscard]] decltype(auto) rpc() noexcept { return impl1.first(); }

    [[nodiscard]] decltype(auto) stop_context() noexcept { return impl1.second(); }

    [[nodiscard]] decltype(auto) service() noexcept { return impl2.first(); }

    [[nodiscard]] decltype(auto) request_handler() noexcept { return impl2.second(); }

    [[nodiscard]] decltype(auto) get_allocator() noexcept { return detail::get_allocator(request_handler()); }

    [[nodiscard]] decltype(auto) get_executor() noexcept { return detail::get_scheduler(request_handler()); }

  private:
    detail::CompressedPair<RPC, detail::Empty> impl1;
    detail::CompressedPair<Service&, RequestHandler> impl2;
};

template <class Operation>
struct RequestRepeater : detail::TypeErasedGrpcTagOperation
{
    using Base = detail::TypeErasedGrpcTagOperation;
    using executor_type = decltype(detail::get_scheduler(std::declval<Operation&>().handler()));
    using RPCContext = detail::RPCContextForRPCT<decltype(std::declval<Operation>().handler().context.rpc())>;

    Operation& operation;
    RPCContext rpc_context;

    explicit RequestRepeater(Operation& operation) : Base(&RequestRepeater::do_complete), operation(operation) {}

    static void do_complete(Base* op, detail::InvokeHandler invoke_handler, bool ok, detail::GrpcContextLocalAllocator);

    [[nodiscard]] decltype(auto) server_context() noexcept { return rpc_context.server_context(); }

    [[nodiscard]] decltype(auto) args() noexcept { return rpc_context.args(); }

    [[nodiscard]] decltype(auto) request() noexcept { return rpc_context.request(); }

    [[nodiscard]] decltype(auto) responder() noexcept { return rpc_context.responder(); }

    [[nodiscard]] auto& context() const noexcept { return operation.handler().context; }

    [[nodiscard]] decltype(auto) get_executor() const noexcept { return detail::get_scheduler(operation.handler()); }

    [[nodiscard]] agrpc::GrpcContext& grpc_context() const noexcept
    {
        return detail::query_grpc_context(context().get_executor());
    }

    [[nodiscard]] auto get_allocator() const noexcept { return context().get_allocator(); }

#ifdef AGRPC_UNIFEX
    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const RequestRepeater& self) noexcept
    {
        return self.get_allocator();
    }
#endif
};

template <class RPC, class Service, class Request, class Responder>
void initiate_request_from_rpc_context(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                       detail::MultiArgRPCContext<Request, Responder>& rpc_context,
                                       grpc::ServerCompletionQueue* cq, void* tag)
{
    (service.*rpc)(&rpc_context.server_context(), &rpc_context.request(), &rpc_context.responder(), cq, cq, tag);
}

template <class RPC, class Service, class Responder>
void initiate_request_from_rpc_context(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                       detail::SingleArgRPCContext<Responder>& rpc_context,
                                       grpc::ServerCompletionQueue* cq, void* tag)
{
    (service.*rpc)(&rpc_context.server_context(), &rpc_context.responder(), cq, cq, tag);
}

template <class Operation>
auto initiate_request_with_repeater(Operation& operation)
{
    auto& context = operation.handler().context;
    auto& grpc_context = detail::query_grpc_context(context.get_executor());
    if (grpc_context.is_stopped())
    {
        return false;
    }
    auto repeater = detail::allocate<detail::RequestRepeater<Operation>>(context.get_allocator(), operation);
    auto* cq = grpc_context.get_server_completion_queue();
    grpc_context.work_started();
    detail::initiate_request_from_rpc_context(context.rpc(), context.service(), repeater->rpc_context, cq,
                                              repeater.get());
    repeater.release();
    return true;
}

template <class Operation>
void RequestRepeater<Operation>::do_complete(Base* op, detail::InvokeHandler invoke_handler, bool ok,
                                             detail::GrpcContextLocalAllocator local_allocator)
{
    auto* self = static_cast<RequestRepeater*>(op);
    detail::AllocatedPointer ptr{self, self->get_allocator()};
    auto& grpc_context = self->grpc_context();
    auto& request_handler = self->context().request_handler();
    auto& operation = self->operation;
    if (detail::InvokeHandler::YES == invoke_handler)
    {
        if (ok)
        {
            const auto is_repeated = detail::initiate_request_with_repeater(operation);
            detail::ScopeGuard guard{[&]
                                     {
                                         if (!is_repeated)
                                         {
                                             detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
                                         }
                                     }};
            request_handler(detail::RepeatedlyRequestContextAccess::create(std::move(ptr)));
        }
        else
        {
            ptr.reset();
            detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
        }
    }
    else
    {
        ptr.reset();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        operation.complete(invoke_handler, local_allocator);
    }
}

struct InitiateRepeatOperation
{
    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* operation)
    {
        if (!detail::initiate_request_with_repeater(*operation))
        {
            detail::GrpcContextImplementation::add_operation(grpc_context, operation);
        }
    }
};

template <class RPC, class Service, class RequestHandler>
struct RepeatedlyRequestInitiator
{
    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler, RPC rpc, Service& service,
                    RequestHandler request_handler) const
    {
        struct CompletionHandlerWithPayload : detail::WorkTrackingCompletionHandler<CompletionHandler>
        {
            detail::RepeatedlyRequestContext<RPC, Service, RequestHandler> context;

            explicit CompletionHandlerWithPayload(CompletionHandler completion_handler, RPC rpc, Service& service,
                                                  RequestHandler request_handler)
                : detail::WorkTrackingCompletionHandler<CompletionHandler>(std::move(completion_handler)),
                  context(rpc, service, std::move(request_handler))
            {
            }
        };
        const auto executor = detail::get_scheduler(request_handler);
        const auto allocator = detail::get_allocator(request_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        detail::create_no_arg_operation<true>(
            grpc_context,
            CompletionHandlerWithPayload{std::move(completion_handler), rpc, service, std::move(request_handler)},
            detail::InitiateRepeatOperation{}, detail::InitiateRepeatOperation{}, allocator);
    }
};
#endif

template <class RPC, class Service, class RequestHandler, class CompletionToken>
auto RepeatedlyRequestFn::impl(RPC rpc, Service& service, RequestHandler request_handler, CompletionToken token)
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    using RPCContext = detail::RPCContextForRPCT<RPC>;
    if constexpr (detail::IS_REPEATEDLY_REQUEST_SENDER_FACTORY<RequestHandler, typename RPCContext::Signature>)
    {
#endif
        return detail::RepeatedlyRequestSender{token.grpc_context, rpc, service, std::move(request_handler)};
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    }
    else
    {
        return asio::async_initiate<CompletionToken, void()>(
            detail::RepeatedlyRequestInitiator<RPC, Service, RequestHandler>{}, token, rpc, service,
            std::move(request_handler));
    }
#endif
}

template <class RPC, class Service, class Request, class Responder, class RequestHandler, class CompletionToken>
auto RepeatedlyRequestFn::operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                     RequestHandler request_handler, CompletionToken token) const
{
    return impl(rpc, service, std::move(request_handler), std::move(token));
}

template <class RPC, class Service, class Responder, class RequestHandler, class CompletionToken>
auto RepeatedlyRequestFn::operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                     RequestHandler request_handler, CompletionToken token) const
{
    return impl(rpc, service, std::move(request_handler), std::move(token));
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
