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

#ifndef AGRPC_DETAIL_REGISTER_RPC_HANDLER_ASIO_BASE_HPP
#define AGRPC_DETAIL_REGISTER_RPC_HANDLER_ASIO_BASE_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/buffer_allocator.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
#include <agrpc/detail/register_rpc_handler_base.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
inline constexpr auto REGISTER_RPC_HANDLER_COMPLETE =
    static_cast<detail::OperationResult>(detail::to_underlying(detail::OperationResult::OK) + 1);

template <class ServerRPC, class RPCHandler, class CompletionHandlerT>
class RegisterRPCHandlerOperationAsioBase
    : public detail::RegisterRPCHandlerOperationBase<ServerRPC, RPCHandler,
                                                     detail::CancellationSlotT<CompletionHandlerT&>>,
      public detail::QueueableOperationBase,
      private detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandlerT>>
{
  public:
    using CompletionHandler = CompletionHandlerT;
    using StopToken = detail::CancellationSlotT<CompletionHandlerT&>;

  private:
    using Base = detail::RegisterRPCHandlerOperationBase<ServerRPC, RPCHandler, StopToken>;
    using WorkTracker = detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandlerT>>;

    struct Decrementer
    {
        void operator()()
        {
            if (self_.decrement_ref_count())
            {
                self_.complete(REGISTER_RPC_HANDLER_COMPLETE, self_.grpc_context());
            }
        }

        RegisterRPCHandlerOperationAsioBase& self_;
    };

  public:
    using typename Base::ServerRPCExecutor;
    using typename Base::Service;
    using Executor = detail::AssociatedExecutorT<CompletionHandlerT, ServerRPCExecutor>;
    using Allocator = detail::AssociatedAllocatorT<CompletionHandlerT>;
    using RPCRequest = detail::RPCRequest<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>;
    using RefCountGuard = detail::ScopeGuard<Decrementer>;

    template <class Ch>
    RegisterRPCHandlerOperationAsioBase(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler,
                                        Ch&& completion_handler, detail::OperationOnComplete on_complete)
        : Base(executor, service, static_cast<RPCHandler&&>(rpc_handler)),
          detail::QueueableOperationBase(on_complete),
          WorkTracker(asio::get_associated_executor(completion_handler)),
          completion_handler_(static_cast<Ch&&>(completion_handler))
    {
        this->grpc_context().work_started();
        this->stop_context_.emplace(detail::get_cancellation_slot(completion_handler_));
    }

    decltype(auto) get_allocator() noexcept { return asio::get_associated_allocator(completion_handler_); }

    CompletionHandlerT& completion_handler() noexcept { return completion_handler_; }

    WorkTracker& work_tracker() noexcept { return *this; }

    CompletionHandlerT completion_handler_;
};

template <class ServerRPC, template <class, class, class> class Operation>
struct RegisterRPCHandlerInitiator
{
    template <class CompletionHandler, class RPCHandler>
    void operator()(CompletionHandler&& completion_handler, const typename ServerRPC::executor_type& executor,
                    RPCHandler&& rpc_handler) const
    {
        using Ch = detail::RemoveCrefT<CompletionHandler>;
        using DecayedRPCHandler = detail::RemoveCrefT<RPCHandler>;
        const auto allocator = asio::get_associated_allocator(completion_handler);
        detail::allocate<Operation<ServerRPC, DecayedRPCHandler, Ch>>(
            allocator, executor, service_, static_cast<RPCHandler&&>(rpc_handler),
            static_cast<CompletionHandler&&>(completion_handler))
            .release();
    }

    detail::GetServerRPCServiceT<ServerRPC>& service_;
};

template <class Operation>
static void register_rpc_handler_asio_do_complete(detail::OperationBase* operation, detail::OperationResult result,
                                                  agrpc::GrpcContext&)
{
    auto& self = *static_cast<Operation*>(operation);
    detail::AllocationGuard guard{&self, self.get_allocator()};
    if (REGISTER_RPC_HANDLER_COMPLETE == result)
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
        auto eptr{static_cast<std::exception_ptr&&>(self.error())};
        detail::dispatch_complete(guard, static_cast<std::exception_ptr&&>(eptr));
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_RPC_HANDLER_ASIO_BASE_HPP
