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

#ifndef AGRPC_DETAIL_REPEATEDLY_REQUEST_AWAITABLE_HPP
#define AGRPC_DETAIL_REPEATEDLY_REQUEST_AWAITABLE_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

#include <agrpc/bind_allocator.hpp>
#include <agrpc/detail/buffer_allocator.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/repeatedly_request_base.hpp>
#include <agrpc/detail/rpc_context.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/repeatedly_request_context.hpp>
#include <agrpc/rpc.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#ifdef AGRPC_HAS_CONCEPTS
template <class Executor, class T>
concept IS_CO_SPAWNABLE =
    requires(Executor executor, T t) { asio::co_spawn(executor, static_cast<T&&>(t), detail::NoOp{}); };
#else
template <class Executor, class T, class = void>
inline constexpr bool IS_CO_SPAWNABLE = false;

template <class Executor, class T>
inline constexpr bool IS_CO_SPAWNABLE<
    Executor, T, decltype((void)asio::co_spawn(std::declval<Executor>(), std::declval<T>(), detail::NoOp{}))> = true;
#endif

template <class Function, class Signature, class = void>
inline constexpr bool INVOKE_RESULT_IS_CO_SPAWNABLE = false;

template <class Function, class... Args>
inline constexpr bool INVOKE_RESULT_IS_CO_SPAWNABLE<
    Function, void(Args...),
    std::enable_if_t<
        detail::IS_CO_SPAWNABLE<detail::GetExecutorT<Function>, std::invoke_result_t<Function, Args...>>>> = true;

template <class Buffer>
class BufferOperation : public detail::QueueableOperationBase
{
  public:
    BufferOperation() noexcept : detail::QueueableOperationBase(&BufferOperation::do_complete) {}

    detail::BufferAllocator<std::byte, Buffer> allocator() noexcept
    {
        return detail::BufferAllocator<std::byte, Buffer>{buffer};
    }

  private:
    static void do_complete(detail::OperationBase* op, detail::OperationResult, agrpc::GrpcContext&) noexcept
    {
        detail::destroy_deallocate(static_cast<BufferOperation*>(op), std::allocator<BufferOperation>{});
    }

    Buffer buffer;
};

template <class Buffer>
auto create_allocated_buffer_operation()
{
    using Op = detail::BufferOperation<Buffer>;
    return detail::allocate<Op>(std::allocator<Op>{}).release();
}

template <class Service, class Request, class Responder, class CompletionToken>
auto initiate_request_from_rpc_context(detail::ServerMultiArgRequest<Service, Request, Responder> rpc, Service& service,
                                       detail::MultiArgRPCContext<Request, Responder>& rpc_context,
                                       CompletionToken&& token)
{
    return agrpc::request(rpc, service, rpc_context.server_context(), rpc_context.request(), rpc_context.responder(),
                          static_cast<CompletionToken&&>(token));
}

template <class Service, class Responder, class CompletionToken>
auto initiate_request_from_rpc_context(detail::ServerSingleArgRequest<Service, Responder> rpc, Service& service,
                                       detail::SingleArgRPCContext<Responder>& rpc_context, CompletionToken&& token)
{
    return agrpc::request(rpc, service, rpc_context.server_context(), rpc_context.responder(),
                          static_cast<CompletionToken&&>(token));
}

template <class CompletionToken>
auto initiate_request_from_rpc_context(detail::GenericRPCMarker, grpc::AsyncGenericService& service,
                                       detail::GenericRPCContext& rpc_context, CompletionToken&& token)
{
    return agrpc::request(service, rpc_context.server_context(), rpc_context.responder(),
                          static_cast<CompletionToken&&>(token));
}

template <class RequestHandler, class RPC, class CompletionHandler>
class RepeatedlyRequestCoroutineOperation
    : public detail::QueueableOperationBase,
      public detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, CompletionHandler>
{
  private:
    using Base = detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, CompletionHandler>;
    using NoArgBase = detail::QueueableOperationBase;
    using Service = detail::GetServiceT<RPC>;
    using RPCContext = detail::RPCContextForRPCT<RPC>;
    using Coroutine =
        detail::RebindCoroutineT<detail::InvokeResultFromSignatureT<RequestHandler&, typename RPCContext::Signature>,
                                 void>;
    using UseCoroutine = detail::CoroutineCompletionTokenT<Coroutine>;
    using CoroutineCompletionHandler = detail::CompletionHandlerTypeT<UseCoroutine, void(bool)>;

    static constexpr auto ON_STOP_COMPLETE = detail::DO_COMPLETE_NO_ARG_HANDLER<RepeatedlyRequestCoroutineOperation>;
    static constexpr auto BUFFER_SIZE = sizeof(CoroutineCompletionHandler) + 2 * sizeof(void*);

    using CoroutineCompletionHandlerBuffer =
        detail::ConditionalT<std::is_same_v<detail::CompletionHandlerUnknown, CoroutineCompletionHandler>,
                             detail::DelayedBuffer, detail::StackBuffer<BUFFER_SIZE>>;
    using BufferOp = detail::BufferOperation<CoroutineCompletionHandlerBuffer>;

  public:
    template <class Ch, class Rh>
    RepeatedlyRequestCoroutineOperation(Rh&& request_handler, RPC rpc, Service& service, Ch&& completion_handler,
                                        bool is_stoppable)
        : NoArgBase(ON_STOP_COMPLETE),
          detail::RepeatedlyRequestOperationBase<RequestHandler, RPC, CompletionHandler>(
              static_cast<Rh&&>(request_handler), rpc, service, static_cast<Ch&&>(completion_handler), is_stoppable),
          buffer_operation(detail::create_allocated_buffer_operation<CoroutineCompletionHandlerBuffer>())
    {
        // Count buffer_operation
        this->grpc_context().work_started();
    }

    ~RepeatedlyRequestCoroutineOperation()
    {
        detail::GrpcContextImplementation::add_local_operation(this->grpc_context(), buffer_operation);
    }

    bool initiate_repeatedly_request()
    {
        if AGRPC_UNLIKELY (this->is_stopped())
        {
            return false;
        }
        asio::co_spawn(this->get_executor(), perform_request_and_repeat(), detail::RethrowFirstArg{});
        return true;
    }

  private:
    Coroutine perform_request_and_repeat()
    {
        RPCContext rpc_context;
        detail::ScopeGuard guard{[&]
                                 {
                                     auto& grpc_context = this->grpc_context();
                                     detail::WorkFinishedOnExit on_exit{grpc_context};
                                     ON_STOP_COMPLETE(this, detail::OperationResult::SHUTDOWN_NOT_OK, grpc_context);
                                 }};
        const auto ok = co_await detail::initiate_request_from_rpc_context(
            this->rpc(), this->service(), rpc_context,
            agrpc::AllocatorBinder(buffer_operation->allocator(), UseCoroutine{}));
        guard.release();
        if AGRPC_LIKELY (ok)
        {
            auto local_request_handler = this->request_handler();
            if AGRPC_UNLIKELY (!this->initiate_repeatedly_request())
            {
                detail::GrpcContextImplementation::add_local_operation(this->grpc_context(), this);
            }
            co_await detail::invoke_from_rpc_context(static_cast<RequestHandler&&>(local_request_handler), rpc_context);
        }
        else
        {
            detail::GrpcContextImplementation::add_local_operation(this->grpc_context(), this);
        }
    }

    BufferOp* buffer_operation;
};
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_REPEATEDLY_REQUEST_AWAITABLE_HPP
