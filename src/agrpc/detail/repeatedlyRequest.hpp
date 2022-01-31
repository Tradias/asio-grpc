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

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
class RepeatedlyRequestStopFunction
{
  public:
    explicit RepeatedlyRequestStopFunction(std::atomic_bool& stopped) noexcept : stopped(stopped) {}

    void operator()(asio::cancellation_type type) noexcept
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            stopped.store(true, std::memory_order_relaxed);
        }
    }

  private:
    std::atomic_bool& stopped;
};
#endif

template <class RPC, class Service, class RequestHandler, bool IsStoppable>
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

    [[nodiscard]] constexpr bool is_stopped() const noexcept
    {
        if constexpr (IsStoppable)
        {
            return impl1.second().load(std::memory_order_relaxed);
        }
        else
        {
            return false;
        }
    }

  private:
    using StopContext = std::conditional_t<IsStoppable, std::atomic_bool, detail::Empty>;

    detail::CompressedPair<RPC, StopContext> impl1;
    detail::CompressedPair<Service&, RequestHandler> impl2;
};

template <class Operation>
struct RequestRepeater : detail::TypeErasedGrpcTagOperation
{
    using Base = detail::TypeErasedGrpcTagOperation;
    using executor_type = decltype(detail::get_scheduler(std::declval<Operation&>().handler()));
    using RPCContext = detail::RPCContextForRPCT<decltype(std::declval<Operation>().handler().context().rpc())>;

    Operation& operation;
    RPCContext rpc_context;

    explicit RequestRepeater(Operation& operation) : Base(&RequestRepeater::do_complete), operation(operation) {}

    static void do_complete(Base* op, detail::InvokeHandler invoke_handler, bool ok, detail::GrpcContextLocalAllocator);

    [[nodiscard]] decltype(auto) server_context() noexcept { return rpc_context.server_context(); }

    [[nodiscard]] decltype(auto) args() noexcept { return rpc_context.args(); }

    [[nodiscard]] decltype(auto) request() noexcept { return rpc_context.request(); }

    [[nodiscard]] decltype(auto) responder() noexcept { return rpc_context.responder(); }

    [[nodiscard]] auto& context() const noexcept { return operation.handler().context(); }

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
    auto& context = operation.handler().context();
    auto& grpc_context = detail::query_grpc_context(context.get_executor());
    if AGRPC_UNLIKELY (context.is_stopped() || grpc_context.is_stopped())
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
    if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
    {
        if AGRPC_LIKELY (ok)
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

template <class CompletionHandler, class RPC, class Service, class RequestHandler, bool IsStoppable>
class RepeatedlyRequestCompletionHandler
{
  public:
    using executor_type = asio::associated_executor_t<CompletionHandler>;
    using allocator_type = asio::associated_allocator_t<CompletionHandler>;
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    using cancellation_slot = asio::associated_cancellation_slot_t<CompletionHandler>;
#endif

    RepeatedlyRequestCompletionHandler(CompletionHandler completion_handler, RPC rpc, Service& service,
                                       RequestHandler request_handler)
        : impl(detail::SecondThenVariadic{}, std::move(completion_handler), rpc, service, std::move(request_handler))
    {
    }

    [[nodiscard]] decltype(auto) context() noexcept { return impl.first(); }

    [[nodiscard]] decltype(auto) get_completion_handler() && { return std::move(impl.second()); }

    [[nodiscard]] executor_type get_executor() const noexcept { return asio::get_associated_executor(impl.second()); }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return asio::get_associated_allocator(impl.second());
    }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    [[nodiscard]] cancellation_slot get_cancellation_slot() const noexcept
    {
        return asio::get_associated_cancellation_slot(impl.second());
    }
#endif

  private:
    detail::CompressedPair<detail::RepeatedlyRequestContext<RPC, Service, RequestHandler, IsStoppable>,
                           detail::WorkTrackingCompletionHandler<CompletionHandler>>
        impl;
};

struct InitiateRepeatOperation
{
    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* operation) const
    {
        if (!detail::initiate_request_with_repeater(*operation))
        {
            detail::GrpcContextImplementation::add_operation(grpc_context, operation);
        }
    }
};

template <class CancellationSlot>
struct InitiateRepeatOperationWithCancellationSlot
{
    CancellationSlot& slot;

    constexpr explicit InitiateRepeatOperationWithCancellationSlot(CancellationSlot& slot) noexcept : slot(slot) {}

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* operation) const
    {
        slot.template emplace<detail::RepeatedlyRequestStopFunction>(operation->handler().context().stop_context());
        detail::InitiateRepeatOperation{}(grpc_context, operation);
    }
};

template <class RPC, class Service, class RequestHandler>
struct RepeatedlyRequestInitiator
{
    template <class CompletionHandler>
    void operator()(CompletionHandler completion_handler, RPC rpc, Service& service,
                    RequestHandler request_handler) const
    {
        const auto executor = detail::get_scheduler(request_handler);
        const auto allocator = detail::get_allocator(request_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
        if (cancellation_slot.is_connected())
        {
            detail::create_no_arg_operation<true, detail::RepeatedlyRequestCompletionHandler<
                                                      CompletionHandler, RPC, Service, RequestHandler, true>>(
                grpc_context, detail::InitiateRepeatOperationWithCancellationSlot{cancellation_slot},
                detail::InitiateRepeatOperationWithCancellationSlot{cancellation_slot}, allocator,
                std::move(completion_handler), rpc, service, std::move(request_handler));
        }
        else
#endif
        {
            detail::create_no_arg_operation<true, detail::RepeatedlyRequestCompletionHandler<
                                                      CompletionHandler, RPC, Service, RequestHandler, false>>(
                grpc_context, detail::InitiateRepeatOperation{}, detail::InitiateRepeatOperation{}, allocator,
                std::move(completion_handler), rpc, service, std::move(request_handler));
        }
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
