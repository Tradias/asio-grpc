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

#ifndef AGRPC_DETAIL_REGISTER_REQUEST_HANDLER_ASIO_BASE_HPP
#define AGRPC_DETAIL_REGISTER_REQUEST_HANDLER_ASIO_BASE_HPP

#include <agrpc/detail/buffer_allocator.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
#include <agrpc/detail/register_request_handler_base.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/grpc_context.hpp>

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/spawn.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/spawn.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
inline constexpr auto REGISTER_REQUEST_HANDLER_COMPLETE =
    static_cast<detail::OperationResult>(detail::to_underlying(detail::OperationResult::OK) + 1);

template <class ServerRPC, class RequestHandler, class CompletionHandlerT>
class RegisterRequestHandlerOperationAsioBase
    : public detail::RegisterRequestHandlerOperationBase<ServerRPC, RequestHandler,
                                                         exec::stop_token_type_t<CompletionHandlerT&>>,
      public detail::QueueableOperationBase
{
  public:
    using CompletionHandler = CompletionHandlerT;
    using StopToken = exec::stop_token_type_t<CompletionHandlerT&>;

  private:
    using Base = detail::RegisterRequestHandlerOperationBase<ServerRPC, RequestHandler, StopToken>;

    struct Decrementer
    {
        void operator()()
        {
            if (self_.decrement_ref_count())
            {
                self_.complete(REGISTER_REQUEST_HANDLER_COMPLETE, self_.grpc_context());
            }
        }

        RegisterRequestHandlerOperationAsioBase& self_;
    };

  public:
    using typename Base::Service;
    using Executor = asio::associated_executor_t<CompletionHandlerT, agrpc::GrpcExecutor>;
    using Allocator = asio::associated_allocator_t<CompletionHandlerT>;
    using RPCRequest = detail::RPCRequest<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>;
    using RefCountGuard = detail::ScopeGuard<Decrementer>;

    template <class Ch>
    RegisterRequestHandlerOperationAsioBase(agrpc::GrpcContext& grpc_context, Service& service,
                                            RequestHandler&& request_handler, Ch&& completion_handler,
                                            detail::OperationOnComplete on_complete)
        : Base(grpc_context, service, static_cast<RequestHandler&&>(request_handler)),
          detail::QueueableOperationBase(on_complete),
          completion_handler_(static_cast<Ch&&>(completion_handler))
    {
        grpc_context.work_started();
        this->stop_context_.emplace(exec::get_stop_token(completion_handler_));
    }

    decltype(auto) get_allocator() noexcept { return exec::get_allocator(completion_handler_); }

    auto& completion_handler() noexcept { return completion_handler_; }

    CompletionHandler completion_handler_;
};

template <class ServerRPC, template <class, class, class> class Operation>
struct RegisterRequestHandlerInitiator
{
    template <class CompletionHandler, class RequestHandler>
    void operator()(CompletionHandler&& completion_handler, const typename ServerRPC::executor_type& executor,
                    detail::GetServerRPCServiceT<ServerRPC>& service, RequestHandler&& request_handler) const
    {
        using TrackingCompletionHandler = detail::WorkTrackingCompletionHandler<CompletionHandler>;
        using DecayedRequestHandler = detail::RemoveCrefT<RequestHandler>;
        auto& grpc_context = detail::query_grpc_context(executor);
        const auto allocator = exec::get_allocator(completion_handler);
        detail::allocate<Operation<ServerRPC, DecayedRequestHandler, TrackingCompletionHandler>>(
            allocator, grpc_context, service, static_cast<RequestHandler&&>(request_handler),
            static_cast<CompletionHandler&&>(completion_handler))
            .release();
    }
};

template <class Operation>
static void register_request_handler_asio_do_complete(detail::OperationBase* operation, detail::OperationResult result,
                                                      agrpc::GrpcContext&)
{
    auto& self = *static_cast<Operation*>(operation);
    detail::AllocationGuard guard{&self, self.get_allocator()};
    if (REGISTER_REQUEST_HANDLER_COMPLETE == result)
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
        using CompletionHandler = typename Operation::CompletionHandler;
        auto handler{static_cast<CompletionHandler&&>(self.completion_handler())};
        auto eptr{static_cast<std::exception_ptr&&>(self.error())};
        guard.reset();
        static_cast<CompletionHandler&&>(handler)(static_cast<std::exception_ptr&&>(eptr));
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_REQUEST_HANDLER_ASIO_BASE_HPP
