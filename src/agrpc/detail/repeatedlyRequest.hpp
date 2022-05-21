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

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/queryGrpcContext.hpp>
#include <agrpc/detail/rpcContext.hpp>
#include <agrpc/detail/typeErasedOperation.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/detail/workTrackingCompletionHandler.hpp>
#include <agrpc/repeatedlyRequestContext.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
#include <agrpc/bindAllocator.hpp>
#include <agrpc/detail/oneShotAllocator.hpp>
#include <agrpc/rpc.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RepeatedlyRequestContextAccess
{
    template <class ImplementationAllocator>
    static auto create(detail::AllocatedPointer<ImplementationAllocator>&& allocated_pointer) noexcept
    {
        return agrpc::RepeatedlyRequestContext{std::move(allocated_pointer)};
    }
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

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

template <class RequestHandler, class RPC, class Service, class CompletionHandler, bool IsStoppable>
class RepeatedlyRequestOperationBase
{
  public:
    template <class Ch, class Rh>
    RepeatedlyRequestOperationBase(Rh&& request_handler, RPC rpc, Service& service, Ch&& completion_handler)
        : request_handler_(std::forward<Rh>(request_handler)),
          impl1(rpc),
          impl2(service, std::forward<Ch>(completion_handler))
    {
    }

    [[nodiscard]] auto& stop_context() noexcept { return impl1.second(); }

    [[nodiscard]] auto& completion_handler() noexcept { return impl2.second(); }

    [[nodiscard]] decltype(auto) get_allocator() noexcept
    {
        return detail::query_allocator(this->request_handler(), this->get_executor());
    }

  protected:
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

    [[nodiscard]] decltype(auto) get_executor() noexcept
    {
        return detail::exec::get_scheduler(this->request_handler());
    }

    [[nodiscard]] agrpc::GrpcContext& grpc_context() noexcept
    {
        return detail::query_grpc_context(this->get_executor());
    }

    [[nodiscard]] RPC rpc() noexcept { return impl1.first(); }

    [[nodiscard]] Service& service() noexcept { return impl2.first(); }

    [[nodiscard]] RequestHandler& request_handler() noexcept { return request_handler_; }

  private:
    using StopContext = detail::ConditionalT<IsStoppable, std::atomic_bool, detail::Empty>;

    RequestHandler request_handler_;
    detail::CompressedPair<RPC, StopContext> impl1;
    detail::CompressedPair<Service&, CompletionHandler> impl2;
};

template <class RequestHandler, class RPC, class Service, class CompletionHandler, bool IsStoppable>
class RepeatedlyRequestOperation
    : public detail::TypeErasedGrpcTagOperation,
      public detail::TypeErasedNoArgOperation,
      public detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, Service, CompletionHandler, IsStoppable>
{
  private:
    using GrpcBase = detail::TypeErasedGrpcTagOperation;
    using NoArgBase = detail::TypeErasedNoArgOperation;
    using RPCContext = detail::RPCContextForRPCT<RPC>;

    static constexpr auto ON_STOP_COMPLETE =
        &detail::default_do_complete<RepeatedlyRequestOperation, detail::TypeErasedNoArgOperation>;

  public:
    template <class Ch, class Rh>
    RepeatedlyRequestOperation(Rh&& request_handler, RPC rpc, Service& service, Ch&& completion_handler)
        : GrpcBase(&RepeatedlyRequestOperation::on_request_complete),
          NoArgBase(ON_STOP_COMPLETE),
          detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, Service, CompletionHandler, IsStoppable>(
              std::forward<Rh>(request_handler), rpc, service, std::forward<Ch>(completion_handler))
    {
    }

    bool initiate_repeatedly_request()
    {
        auto& local_grpc_context = this->grpc_context();
        if AGRPC_UNLIKELY (this->is_stopped())
        {
            return false;
        }
        auto next_rpc_context = this->allocate_rpc_context();
        auto* cq = local_grpc_context.get_server_completion_queue();
        local_grpc_context.work_started();
        detail::initiate_request_from_rpc_context(this->rpc(), this->service(), *next_rpc_context, cq, this);
        next_rpc_context.release();
        return true;
    }

  private:
    static void on_request_complete(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok,
                                    detail::GrpcContextLocalAllocator local_allocator)
    {
        auto* self = static_cast<RepeatedlyRequestOperation*>(op);
        detail::AllocatedPointer ptr{self->rpc_context, self->get_allocator()};
        auto& grpc_context = self->grpc_context();
        auto& request_handler = self->request_handler();
        if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
        {
            if AGRPC_LIKELY (ok)
            {
                const auto is_repeated = self->initiate_repeatedly_request();
                detail::ScopeGuard guard{[&]
                                         {
                                             if AGRPC_UNLIKELY (!is_repeated)
                                             {
                                                 detail::GrpcContextImplementation::add_local_operation(grpc_context,
                                                                                                        self);
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

    auto allocate_rpc_context()
    {
        auto new_rpc_context = detail::allocate<RPCContext>(this->get_allocator());
        this->rpc_context = new_rpc_context.get();
        return new_rpc_context;
    }

    RPCContext* rpc_context;
};

template <class Operation>
void initiate_repeatedly_request(agrpc::GrpcContext& grpc_context, Operation& operation)
{
    if (!operation.initiate_repeatedly_request())
    {
        detail::GrpcContextImplementation::add_operation(grpc_context, &operation);
    }
}

template <template <class, class, class, class, bool> class Operation>
struct BasicRepeatedlyRequestInitiator
{
    template <class RequestHandler, class RPC, class Service, class CompletionHandler>
    void operator()(CompletionHandler&& completion_handler, RequestHandler&& request_handler, RPC rpc,
                    Service& service) const
    {
        using TrackingCompletionHandler = detail::WorkTrackingCompletionHandler<CompletionHandler>;
        using DecayedRequestHandler = std::decay_t<RequestHandler>;
        const auto [executor, allocator] = detail::get_associated_executor_and_allocator(request_handler);
        auto& grpc_context = detail::query_grpc_context(executor);
        grpc_context.work_started();
        detail::WorkFinishedOnExit on_exit{grpc_context};
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if (auto cancellation_slot = asio::get_associated_cancellation_slot(completion_handler);
            cancellation_slot.is_connected())
        {
            auto operation =
                detail::allocate<Operation<DecayedRequestHandler, RPC, Service, TrackingCompletionHandler, true>>(
                    allocator, std::forward<RequestHandler>(request_handler), rpc, service,
                    std::forward<CompletionHandler>(completion_handler));
            cancellation_slot.template emplace<detail::RepeatedlyRequestStopFunction>(operation->stop_context());
            detail::initiate_repeatedly_request(grpc_context, *operation);
            operation.release();
        }
        else
#endif
        {
            auto operation =
                detail::allocate<Operation<DecayedRequestHandler, RPC, Service, TrackingCompletionHandler, false>>(
                    allocator, std::forward<RequestHandler>(request_handler), rpc, service,
                    std::forward<CompletionHandler>(completion_handler));
            detail::initiate_repeatedly_request(grpc_context, *operation);
            operation.release();
        }
        on_exit.release();
    }
};

using RepeatedlyRequestInitiator = detail::BasicRepeatedlyRequestInitiator<detail::RepeatedlyRequestOperation>;

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class Executor, class T, class = void>
inline constexpr bool IS_CO_SPAWNABLE = false;

template <class Executor, class T>
inline constexpr bool IS_CO_SPAWNABLE<
    Executor, T, std::void_t<decltype(asio::co_spawn(std::declval<Executor>(), std::declval<T>(), detail::NoOp{}))>> =
    true;

template <class Function, class Signature, class = void>
inline constexpr bool INVOKE_RESULT_IS_CO_SPAWNABLE = false;

template <class Function, class... Args>
inline constexpr bool INVOKE_RESULT_IS_CO_SPAWNABLE<
    Function, void(Args...),
    std::enable_if_t<detail::IS_CO_SPAWNABLE<detail::SchedulerT<Function>, std::invoke_result_t<Function, Args...>>>> =
    true;

struct RethrowFirstArg
{
    template <class... Args>
    void operator()(std::exception_ptr ep, Args&&...) const
    {
        if AGRPC_UNLIKELY (ep)
        {
            std::rethrow_exception(ep);
        }
    }
};

template <class CompletionToken, class Signature, class = void>
struct CompletionHandlerType
{
    using Type = typename asio::async_result<CompletionToken, Signature>::completion_handler_type;
};

template <class CompletionToken, class Signature>
struct CompletionHandlerType<CompletionToken, Signature,
                             std::void_t<typename asio::async_result<CompletionToken, Signature>::handler_type>>
{
    using Type = typename asio::async_result<CompletionToken, Signature>::handler_type;
};

template <class CompletionToken, class Signature>
using CompletionHandlerTypeT = typename CompletionHandlerType<CompletionToken, Signature>::Type;

template <std::size_t BufferSize>
class BufferOperation : public detail::TypeErasedNoArgOperation
{
  public:
    BufferOperation() noexcept : detail::TypeErasedNoArgOperation(&do_complete) {}

    auto one_shot_allocator() noexcept { return detail::OneShotAllocator<std::byte, BufferSize>{&this->buffer}; }

  private:
    static void do_complete(detail::TypeErasedNoArgOperation* op, detail::InvokeHandler,
                            detail::GrpcContextLocalAllocator) noexcept
    {
        detail::deallocate(std::allocator<BufferOperation>{}, static_cast<BufferOperation*>(op));
    }

    std::aligned_storage_t<BufferSize> buffer;
};

template <std::size_t BufferSize>
auto create_allocated_buffer_operation()
{
    using Op = detail::BufferOperation<BufferSize>;
    auto ptr = detail::allocate<Op>(std::allocator<Op>{});
    auto* buffer_operation_ptr = ptr.get();
    ptr.release();
    return buffer_operation_ptr;
}

template <class RPC, class Service, class Request, class Responder, class CompletionToken>
auto initiate_request_from_rpc_context(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                       detail::MultiArgRPCContext<Request, Responder>& rpc_context,
                                       CompletionToken&& token)
{
    return agrpc::request(rpc, service, rpc_context.server_context(), rpc_context.request(), rpc_context.responder(),
                          std::forward<CompletionToken>(token));
}

template <class RPC, class Service, class Responder, class CompletionToken>
auto initiate_request_from_rpc_context(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                       detail::SingleArgRPCContext<Responder>& rpc_context, CompletionToken&& token)
{
    return agrpc::request(rpc, service, rpc_context.server_context(), rpc_context.responder(),
                          std::forward<CompletionToken>(token));
}

template <class CompletionToken>
auto initiate_request_from_rpc_context(detail::GenericRPCMarker, grpc::AsyncGenericService& service,
                                       detail::GenericRPCContext& rpc_context, CompletionToken&& token)
{
    return agrpc::request(service, rpc_context.server_context(), rpc_context.responder(),
                          std::forward<CompletionToken>(token));
}

template <class RequestHandler, class RPC, class Service, class CompletionHandler, bool IsStoppable>
class RepeatedlyRequestAwaitableOperation
    : public detail::TypeErasedNoArgOperation,
      public detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, Service, CompletionHandler, IsStoppable>
{
  private:
    using Base = detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, Service, CompletionHandler, IsStoppable>;
    using NoArgBase = detail::TypeErasedNoArgOperation;
    using RPCContext = detail::RPCContextForRPCT<RPC>;
    using Awaitable = detail::InvokeResultFromSignatureT<RequestHandler&, typename RPCContext::Signature>;
    using UseAwaitable = asio::use_awaitable_t<typename Awaitable::executor_type>;

    static constexpr auto ON_STOP_COMPLETE =
        &detail::default_do_complete<RepeatedlyRequestAwaitableOperation, detail::TypeErasedNoArgOperation>;
    static constexpr auto BUFFER_SIZE =
        sizeof(detail::CompletionHandlerTypeT<UseAwaitable, void()>) + 3 * sizeof(void*);

    using BufferOp = detail::BufferOperation<BUFFER_SIZE>;

  public:
    template <class Ch, class Rh>
    RepeatedlyRequestAwaitableOperation(Rh&& request_handler, RPC rpc, Service& service, Ch&& completion_handler)
        : NoArgBase(ON_STOP_COMPLETE),
          detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, Service, CompletionHandler, IsStoppable>(
              std::forward<Rh>(request_handler), rpc, service, std::forward<Ch>(completion_handler)),
          buffer_operation(detail::create_allocated_buffer_operation<BUFFER_SIZE>())
    {
        // Count buffer_operation
        this->grpc_context().work_started();
    }

    ~RepeatedlyRequestAwaitableOperation()
    {
        detail::GrpcContextImplementation::add_local_operation(this->grpc_context(), this->buffer_operation);
    }

    bool initiate_repeatedly_request()
    {
        if AGRPC_UNLIKELY (this->is_stopped())
        {
            return false;
        }
        asio::co_spawn(this->get_executor(), this->perform_request_and_repeat(), detail::RethrowFirstArg{});
        return true;
    }

  private:
    Awaitable perform_request_and_repeat()
    {
        auto& local_grpc_context = this->grpc_context();
        RPCContext rpc_context;
        detail::ScopeGuard guard{[&]
                                 {
                                     detail::WorkFinishedOnExit on_exit{local_grpc_context};
                                     ON_STOP_COMPLETE(this, detail::InvokeHandler::NO, {});
                                 }};
        const auto ok = co_await detail::initiate_request_from_rpc_context(
            this->rpc(), this->service(), rpc_context,
            agrpc::bind_allocator(this->buffer_operation->one_shot_allocator(), UseAwaitable{}));
        guard.release();
        if AGRPC_LIKELY (ok)
        {
            auto local_request_handler = this->request_handler();
            if AGRPC_UNLIKELY (!this->initiate_repeatedly_request())
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

    BufferOp* buffer_operation;
};

using RepeatedlyRequestAwaitableInitiator =
    detail::BasicRepeatedlyRequestInitiator<detail::RepeatedlyRequestAwaitableOperation>;
#endif
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUEST_HPP
