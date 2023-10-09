// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_REGISTER_YIELD_REQUEST_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_YIELD_REQUEST_HANDLER_HPP

#include <agrpc/detail/buffer_allocator.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
#include <agrpc/detail/register_request_handler_base.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/detail/start_server_rpc.hpp>
#include <agrpc/grpc_context.hpp>

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/spawn.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/spawn.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Executor, class Function>
void spawn(Executor&& executor, Function&& function)
{
#ifdef AGRPC_ASIO_HAS_NEW_SPAWN
    asio::spawn(static_cast<Executor&&>(executor), static_cast<Function&&>(function), detail::RethrowFirstArg{});
#else
    asio::spawn(static_cast<Executor&&>(executor), static_cast<Function&&>(function));
#endif
}

template <class ServerRPC, class RequestHandler, class CompletionHandler>
struct YieldRequestHandlerOperation
    : detail::RegisterRequestHandlerOperationBase<ServerRPC, RequestHandler,
                                                  exec::stop_token_type_t<CompletionHandler&>>,
      detail::QueueableOperationBase
{
    static constexpr auto COMPLETE =
        static_cast<detail::OperationResult>(detail::to_underlying(detail::OperationResult::OK) + 1);

    using StopToken = exec::stop_token_type_t<CompletionHandler&>;
    using Base = detail::RegisterRequestHandlerOperationBase<ServerRPC, RequestHandler, StopToken>;
    using typename Base::Service;
    using Executor = asio::associated_executor_t<CompletionHandler, agrpc::GrpcExecutor>;
    using Allocator = asio::associated_allocator_t<CompletionHandler>;
    using RPCRequest = detail::RPCRequest<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>;

#ifdef AGRPC_ASIO_HAS_NEW_SPAWN
    using YieldCompletionHandler = detail::CompletionHandlerTypeT<asio::basic_yield_context<Executor>, void(bool)>;
#else
    using YieldCompletionHandler = detail::CompletionHandlerUnknown;
#endif

    static constexpr auto BUFFER_SIZE = sizeof(YieldCompletionHandler) + 3 * sizeof(void*);

    using YieldCompletionHandlerBuffer =
        detail::ConditionalT<std::is_same_v<detail::CompletionHandlerUnknown, YieldCompletionHandler>,
                             detail::DelayedBuffer, detail::StackBuffer<BUFFER_SIZE>>;

    struct Decrementer
    {
        void operator()()
        {
            if (self_.decrement_ref_count())
            {
                self_.complete(COMPLETE, self_.grpc_context());
            }
        }

        YieldRequestHandlerOperation& self_;
    };

    static void do_complete(detail::OperationBase* operation, detail::OperationResult result, agrpc::GrpcContext&)
    {
        auto& self = *static_cast<YieldRequestHandlerOperation*>(operation);
        detail::AllocationGuard guard{&self, self.get_allocator()};
        if (COMPLETE == result)
        {
            if AGRPC_LIKELY (!detail::GrpcContextImplementation::is_shutdown(self.grpc_context()))
            {
                detail::GrpcContextImplementation::add_operation(self.grpc_context(), &self);
                guard.release();
            }
            return;
        }
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            auto handler{static_cast<CompletionHandler&&>(self.completion_handler_)};
            auto eptr{static_cast<std::exception_ptr&&>(self.error())};
            guard.reset();
            static_cast<CompletionHandler&&>(handler)(static_cast<std::exception_ptr&&>(eptr));
        }
    }

    template <class Ch>
    YieldRequestHandlerOperation(agrpc::GrpcContext& grpc_context, Service& service, RequestHandler&& request_handler,
                                 Ch&& completion_handler)
        : Base(grpc_context, service, static_cast<RequestHandler&&>(request_handler)),
          detail::QueueableOperationBase(&do_complete),
          completion_handler_(static_cast<Ch&&>(completion_handler))
    {
        grpc_context.work_started();
        this->stop_context_.emplace(exec::get_stop_token(completion_handler_));
        initiate();
    }

    void initiate()
    {
        this->increment_ref_count();
        detail::spawn(asio::get_associated_executor(completion_handler_, this->grpc_context()),
                      [g = detail::ScopeGuard<Decrementer>{*this}](const auto& yield)
                      {
                          g.get().self_.perform_request_and_repeat(yield);
                      });
    }

    void initiate_next()
    {
        if AGRPC_LIKELY (!this->is_stopped())
        {
            initiate();
        }
    }

    template <class Yield>
    void perform_request_and_repeat(const Yield& yield)
    {
        auto rpc = detail::ServerRPCContextBaseAccess::construct<ServerRPC>(this->grpc_context().get_executor());
        RPCRequest req;
        if (!req.start(rpc, this->service(), agrpc::AllocatorBinder(detail::BufferAllocator{buffer_}, yield)))
        {
            return;
        }
        initiate_next();
        AGRPC_TRY { req.invoke(this->request_handler(), rpc, yield); }
        AGRPC_CATCH(...)
        {
            this->stop();
            this->set_error(std::current_exception());
        }
        if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
        {
            rpc.cancel();
        }
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (!rpc.is_done())
            {
                rpc.wait_for_done(yield);
            }
        }
    }

    decltype(auto) get_allocator() noexcept { return exec::get_allocator(completion_handler_); }

    YieldCompletionHandlerBuffer buffer_;
    CompletionHandler completion_handler_;
};

template <class ServerRPC>
struct YieldRequestHandlerInitiator
{
    template <class CompletionHandler, class RequestHandler>
    void operator()(CompletionHandler&& completion_handler, const typename ServerRPC::executor_type& executor,
                    detail::GetServerRPCServiceT<ServerRPC>& service, RequestHandler&& request_handler) const
    {
        using TrackingCompletionHandler = detail::WorkTrackingCompletionHandler<CompletionHandler>;
        using DecayedRequestHandler = detail::RemoveCrefT<RequestHandler>;
        auto& grpc_context = detail::query_grpc_context(executor);
        const auto allocator = exec::get_allocator(completion_handler);
        detail::allocate<YieldRequestHandlerOperation<ServerRPC, DecayedRequestHandler, TrackingCompletionHandler>>(
            allocator, grpc_context, service, static_cast<RequestHandler&&>(request_handler),
            static_cast<CompletionHandler&&>(completion_handler))
            .release();
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_YIELD_REQUEST_HANDLER_HPP
