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
template <class Function, class Signature, class = void>
inline constexpr bool INVOKE_RESULT_IS_SENDER = false;

template <class Function, class... Args>
inline constexpr bool INVOKE_RESULT_IS_SENDER<
    Function, void(Args...), std::enable_if_t<detail::is_sender_v<std::invoke_result_t<Function, Args...>>>> = true;

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

template <class CompletionHandler, class RequestHandler, class RPC, class Service, bool IsStoppable>
class RepeatedlyRequestOperation : public detail::TypeErasedGrpcTagOperation, public detail::TypeErasedNoArgOperation
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;
    using NoArgBase = detail::TypeErasedNoArgOperation;
    using RPCContext = detail::RPCContextForRPCT<RPC>;

    static constexpr auto ON_STOP_COMPLETE =
        &detail::default_do_complete<RepeatedlyRequestOperation, detail::TypeErasedNoArgOperation>;

  public:
    template <class Ch>
    RepeatedlyRequestOperation(Ch&& completion_handler, RequestHandler request_handler, RPC rpc, Service& service)
        : GrpcBase(&RepeatedlyRequestOperation::on_request_complete),
          NoArgBase(ON_STOP_COMPLETE),
          request_handler_(std::move(request_handler)),
          impl1(rpc),
          impl2(service, std::forward<Ch>(completion_handler))
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

    [[nodiscard]] decltype(auto) get_allocator() noexcept
    {
        return detail::query_allocator(request_handler(), get_executor());
    }

    [[nodiscard]] auto& completion_handler() noexcept { return impl2.second(); }

  private:
    using StopContext = std::conditional_t<IsStoppable, std::atomic_bool, detail::Empty>;

    static void on_request_complete(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok,
                                    detail::GrpcContextLocalAllocator);

    [[nodiscard]] auto& request_handler() noexcept { return request_handler_; }

    [[nodiscard]] decltype(auto) get_executor() noexcept { return detail::get_scheduler(request_handler()); }

    RequestHandler request_handler_;
    detail::CompressedPair<RPC, StopContext> impl1;
    detail::CompressedPair<Service&, CompletionHandler> impl2;
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
auto initiate_repeatedly_request(Operation& operation)
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
            const auto is_repeated = detail::initiate_repeatedly_request(*self);
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
        ON_STOP_COMPLETE(self, invoke_handler, local_allocator);
    }
}

struct RepeatedlyRequestInitFunction
{
    template <class Operation>
    void operator()(agrpc::GrpcContext& grpc_context, Operation& operation) const
    {
        if (!detail::initiate_repeatedly_request(operation))
        {
            detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
        }
    }
};

template <template <class, class, class, class, bool> class Operation, class InitFunction>
struct BasicRepeatedlyRequestInitiator
{
    template <class CompletionHandler, class RequestHandler, class RPC, class Service>
    void operator()(CompletionHandler&& completion_handler, RequestHandler&& request_handler, RPC rpc,
                    Service& service) const
    {
        using TrackingCompletionHandler = detail::WorkTrackingCompletionHandler<CompletionHandler>;
        const auto [executor, allocator] = detail::get_associated_executor_and_allocator(request_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if (auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
            cancellation_slot.is_connected())
        {
            auto operation = detail::allocate<Operation<TrackingCompletionHandler, RequestHandler, RPC, Service, true>>(
                allocator, std::forward<CompletionHandler>(completion_handler),
                std::forward<RequestHandler>(request_handler), rpc, service);
            cancellation_slot.template emplace<detail::RepeatedlyRequestStopFunction>(operation->stop_context());
            InitFunction{}(grpc_context, *operation);
            operation.release();
        }
        else
#endif
        {
            auto operation =
                detail::allocate<Operation<TrackingCompletionHandler, RequestHandler, RPC, Service, false>>(
                    allocator, std::forward<CompletionHandler>(completion_handler),
                    std::forward<RequestHandler>(request_handler), rpc, service);
            InitFunction{}(grpc_context, *operation);
            operation.release();
        }
        on_exit.release();
    }
};

using RepeatedlyRequestInitiator =
    detail::BasicRepeatedlyRequestInitiator<detail::RepeatedlyRequestOperation, detail::RepeatedlyRequestInitFunction>;

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class T>
inline constexpr bool IS_ASIO_AWAITABLE = false;

template <class T, class Executor>
inline constexpr bool IS_ASIO_AWAITABLE<asio::awaitable<T, Executor>> = true;

template <class Function, class Signature, class = void>
inline constexpr bool INVOKE_RESULT_IS_ASIO_AWAITABLE = false;

template <class Function, class... Args>
inline constexpr bool INVOKE_RESULT_IS_ASIO_AWAITABLE<
    Function, void(Args...), std::enable_if_t<detail::IS_ASIO_AWAITABLE<std::invoke_result_t<Function, Args...>>>> =
    true;

template <class Function, class Signature>
struct InvokeResultFromSignature;

template <class Function, class... Args>
struct InvokeResultFromSignature<Function, void(Args...)>
{
    using Type = std::invoke_result_t<Function, Args...>;
};

template <class Function, class Signature>
using InvokeResultFromSignatureT = typename detail::InvokeResultFromSignature<Function, Signature>::Type;

template <class CompletionHandler, class RequestHandler, class RPC, class Service, bool IsStoppable>
class RepeatedlyRequestAwaitableOperation : public detail::TypeErasedNoArgOperation
{
  private:
    using NoArgBase = detail::TypeErasedNoArgOperation;
    using RPCContext = detail::RPCContextForRPCT<RPC>;
    using Awaitable = detail::InvokeResultFromSignatureT<RequestHandler&, typename RPCContext::Signature>;
    using UseAwaitable = asio::use_awaitable_t<typename Awaitable::executor_type>;

    static constexpr auto ON_STOP_COMPLETE =
        &detail::default_do_complete<RepeatedlyRequestAwaitableOperation, detail::TypeErasedNoArgOperation>;

  public:
    template <class Ch>
    RepeatedlyRequestAwaitableOperation(Ch&& completion_handler, RequestHandler request_handler, RPC rpc,
                                        Service& service)
        : NoArgBase(ON_STOP_COMPLETE),
          request_handler_(std::move(request_handler)),
          impl1(rpc),
          impl2(service, std::forward<Ch>(completion_handler))
    {
    }

    [[nodiscard]] auto& stop_context() noexcept { return impl1.second(); }

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

    Awaitable on_request_complete();

    [[nodiscard]] decltype(auto) get_executor() noexcept { return detail::get_scheduler(request_handler()); }

    [[nodiscard]] decltype(auto) get_allocator() noexcept
    {
        return detail::query_allocator(request_handler(), get_executor());
    }

    [[nodiscard]] auto& completion_handler() noexcept { return impl2.second(); }

  private:
    using StopContext = std::conditional_t<IsStoppable, std::atomic_bool, detail::Empty>;

    [[nodiscard]] auto& rpc() noexcept { return impl1.first(); }

    [[nodiscard]] auto& service() noexcept { return impl2.first(); }

    [[nodiscard]] auto& request_handler() noexcept { return request_handler_; }

    RequestHandler request_handler_;
    detail::CompressedPair<RPC, StopContext> impl1;
    detail::CompressedPair<Service&, CompletionHandler> impl2;
};

struct RethrowFirstArg
{
    template <class... Args>
    void operator()(std::exception_ptr ep, Args&&...) const
    {
        if (ep)
        {
            std::rethrow_exception(ep);
        }
    }
};

template <class Operation>
bool initiate_awaitable_repeatedly_request(Operation& operation)
{
    if AGRPC_UNLIKELY (operation.is_stopped() || operation.grpc_context().is_stopped())
    {
        return false;
    }
    asio::co_spawn(operation.get_executor(), operation.on_request_complete(), detail::RethrowFirstArg{});
    return true;
}

template <class RPC, class Service, class Request, class Responder, class CompletionToken>
auto initiate_request_from_rpc_context(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                       detail::MultiArgRPCContext<Request, Responder>& rpc_context,
                                       CompletionToken token)
{
    return agrpc::request(rpc, service, rpc_context.server_context(), rpc_context.request(), rpc_context.responder(),
                          token);
}

template <class RPC, class Service, class Responder, class CompletionToken>
auto initiate_request_from_rpc_context(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                       detail::SingleArgRPCContext<Responder>& rpc_context, CompletionToken token)
{
    return agrpc::request(rpc, service, rpc_context.server_context(), rpc_context.responder(), token);
}

template <class CompletionHandler, class RPC, class Service, class RequestHandler, bool IsStoppable>
typename RepeatedlyRequestAwaitableOperation<CompletionHandler, RPC, Service, RequestHandler, IsStoppable>::Awaitable
RepeatedlyRequestAwaitableOperation<CompletionHandler, RPC, Service, RequestHandler, IsStoppable>::on_request_complete()
{
    auto& local_grpc_context = grpc_context();
    RPCContext rpc_context;
    detail::ScopeGuard guard{[&]
                             {
                                 detail::WorkFinishedOnExit on_exit{local_grpc_context};
                                 ON_STOP_COMPLETE(this, detail::InvokeHandler::NO, {});
                             }};
    const auto ok = co_await detail::initiate_request_from_rpc_context(rpc(), service(), rpc_context, UseAwaitable{});
    guard.release();
    if AGRPC_LIKELY (ok)
    {
        auto local_request_handler = request_handler();
        if (!detail::initiate_awaitable_repeatedly_request(*this))
        {
            detail::GrpcContextImplementation::add_local_operation(local_grpc_context, this);
        }
        co_await std::apply(std::move(local_request_handler), rpc_context.args());
    }
    else
    {
        detail::GrpcContextImplementation::add_local_operation(local_grpc_context, this);
    }
}

struct RepeatedlyRequestAwaitableInitFunction
{
    template <class Operation>
    void operator()(agrpc::GrpcContext& grpc_context, Operation& operation) const
    {
        if (!detail::initiate_awaitable_repeatedly_request(operation))
        {
            detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
        }
    }
};

using RepeatedlyRequestAwaitableInitiator =
    detail::BasicRepeatedlyRequestInitiator<detail::RepeatedlyRequestAwaitableOperation,
                                            detail::RepeatedlyRequestAwaitableInitFunction>;
#endif
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
