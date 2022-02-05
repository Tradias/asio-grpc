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
#include "agrpc/detail/rpcContext.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/detail/workTrackingCompletionHandler.hpp"
#include "agrpc/initiate.hpp"
#include "agrpc/repeatedlyRequestContext.hpp"
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
inline constexpr bool INVOKE_RESULT_IS_SENDER = false;

template <class Handler, class... Args>
inline constexpr bool INVOKE_RESULT_IS_SENDER<
    Handler, void(Args...), std::enable_if_t<detail::is_sender_v<std::invoke_result_t<Handler&&, Args...>>>> = true;

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class T>
inline constexpr bool IS_ASIO_AWAITABLE = false;

template <class T>
inline constexpr bool IS_ASIO_AWAITABLE<asio::awaitable<T>> = true;

template <class Handler, class Args, class = void>
inline constexpr bool INVOKE_RESULT_IS_ASIO_AWAITABLE = false;

template <class Handler, class... Args>
inline constexpr bool INVOKE_RESULT_IS_ASIO_AWAITABLE<
    Handler, void(Args...), std::enable_if_t<detail::IS_ASIO_AWAITABLE<std::invoke_result_t<Handler&&, Args...>>>> =
    true;
#endif

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

template <class CompletionHandler, class RPC, class Service, class RequestHandler, bool IsStoppable>
class RepeatedlyRequestOperation : public detail::TypeErasedGrpcTagOperation, public detail::TypeErasedNoArgOperation
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;
    using NoArgBase = detail::TypeErasedNoArgOperation;
    using RPCContext = detail::RPCContextForRPCT<RPC>;

  public:
    template <class Ch>
    RepeatedlyRequestOperation(Ch&& completion_handler, RPC rpc, Service& service, RequestHandler request_handler)
        : GrpcBase(&RepeatedlyRequestOperation::on_request_complete),
          NoArgBase(&RepeatedlyRequestOperation::on_stop_complete),
          impl1(rpc),
          impl2(service, std::forward<Ch>(completion_handler)),
          request_handler_(std::move(request_handler))
    {
    }

    [[nodiscard]] auto& rpc() noexcept { return impl1.first(); }

    [[nodiscard]] auto& stop_context() noexcept { return impl1.second(); }

    [[nodiscard]] auto& service() noexcept { return impl2.first(); }

    [[nodiscard]] agrpc::GrpcContext& grpc_context() noexcept { return detail::query_grpc_context(get_executor()); }

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

    [[nodiscard]] auto allocate_rpc_context()
    {
        auto new_rpc_context = detail::allocate<RPCContext>(get_allocator());
        this->rpc_context = new_rpc_context.get();
        return new_rpc_context;
    }

  private:
    using StopContext = std::conditional_t<IsStoppable, std::atomic_bool, detail::Empty>;

    static void on_request_complete(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok,
                                    detail::GrpcContextLocalAllocator);

    static void on_stop_complete(NoArgBase* op, detail::InvokeHandler invoke_handler,
                                 detail::GrpcContextLocalAllocator);

    [[nodiscard]] auto& completion_handler() noexcept { return impl2.second(); }

    [[nodiscard]] auto& request_handler() noexcept { return request_handler_; }

    [[nodiscard]] decltype(auto) get_allocator() noexcept { return detail::get_allocator(request_handler()); }

    [[nodiscard]] decltype(auto) get_executor() noexcept { return detail::get_scheduler(request_handler()); }

    detail::CompressedPair<RPC, StopContext> impl1;
    detail::CompressedPair<Service&, CompletionHandler> impl2;
    RequestHandler request_handler_;
    RPCContext* rpc_context;
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
    auto& grpc_context = operation.grpc_context();
    if AGRPC_UNLIKELY (operation.is_stopped() || grpc_context.is_stopped())
    {
        return false;
    }
    auto rpc_context = operation.allocate_rpc_context();
    auto* cq = grpc_context.get_server_completion_queue();
    grpc_context.work_started();
    detail::initiate_request_from_rpc_context(operation.rpc(), operation.service(), *rpc_context, cq,
                                              std::addressof(operation));
    rpc_context.release();
    return true;
}

template <class CompletionHandler, class RPC, class Service, class RequestHandler, bool IsStoppable>
void RepeatedlyRequestOperation<CompletionHandler, RPC, Service, RequestHandler, IsStoppable>::on_request_complete(
    GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok, detail::GrpcContextLocalAllocator local_allocator)
{
    auto* self = static_cast<RepeatedlyRequestOperation*>(op);
    detail::AllocatedPointer ptr{self->rpc_context, self->get_allocator()};
    auto& grpc_context = self->grpc_context();
    auto& request_handler = self->request_handler();
    if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
    {
        if AGRPC_LIKELY (ok)
        {
            const auto is_repeated = detail::initiate_request_with_repeater(*self);
            detail::ScopeGuard guard{[&]
                                     {
                                         if (!is_repeated)
                                         {
                                             detail::GrpcContextImplementation::add_local_operation(grpc_context, self);
                                         }
                                     }};
            request_handler(detail::RepeatedlyRequestContextAccess::create(std::move(ptr)));
        }
        else
        {
            ptr.reset();
            detail::GrpcContextImplementation::add_local_operation(grpc_context, self);
        }
    }
    else
    {
        ptr.reset();
        detail::WorkFinishedOnExit on_exit{grpc_context};
        on_stop_complete(self, invoke_handler, local_allocator);
    }
}

template <class CompletionHandler, class RPC, class Service, class RequestHandler, bool IsStoppable>
void RepeatedlyRequestOperation<CompletionHandler, RPC, Service, RequestHandler, IsStoppable>::on_stop_complete(
    NoArgBase* op, detail::InvokeHandler invoke_handler, detail::GrpcContextLocalAllocator)
{
    auto* self = static_cast<RepeatedlyRequestOperation*>(op);
    detail::AllocatedPointer ptr{self, self->get_allocator()};
    if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
    {
        auto handler{std::move(self->completion_handler())};
        ptr.reset();
        std::move(handler)();
    }
}

template <class T>
void initiate_repeat_operation(agrpc::GrpcContext& grpc_context, T* operation)
{
    if (!detail::initiate_request_with_repeater(*operation))
    {
        detail::GrpcContextImplementation::add_operation(grpc_context, operation);
    }
}

template <class RPC, class Service, class RequestHandler>
struct RepeatedlyRequestInitiator
{
    template <class CompletionHandler>
    void operator()(CompletionHandler&& completion_handler, RPC rpc, Service& service,
                    RequestHandler request_handler) const
    {
        using TrackingCompletionHandler = detail::WorkTrackingCompletionHandler<CompletionHandler>;
        const auto executor = detail::get_scheduler(request_handler);
        const auto allocator = detail::get_allocator(request_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
        if (cancellation_slot.is_connected())
        {
            auto operation = detail::allocate<
                detail::RepeatedlyRequestOperation<TrackingCompletionHandler, RPC, Service, RequestHandler, true>>(
                allocator, std::forward<CompletionHandler>(completion_handler), rpc, service,
                std::move(request_handler));
            cancellation_slot.template emplace<detail::RepeatedlyRequestStopFunction>(operation->stop_context());
            detail::initiate_repeat_operation(grpc_context, operation.get());
            operation.release();
        }
        else
#endif
        {
            auto operation = detail::allocate<
                detail::RepeatedlyRequestOperation<TrackingCompletionHandler, RPC, Service, RequestHandler, false>>(
                allocator, std::forward<CompletionHandler>(completion_handler), rpc, service,
                std::move(request_handler));
            detail::initiate_repeat_operation(grpc_context, operation.get());
            operation.release();
        }
        on_exit.release();
    }
};
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
