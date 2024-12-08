// Copyright 2024 Dennis Hezel
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

#include <agrpc/alarm.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/register_rpc_handler_base.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>
#include <agrpc/detail/server_rpc_starter.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
void register_rpc_handler_asio_completion_trampoline(agrpc::GrpcContext& grpc_context,
                                                     detail::RegisterRPCHandlerOperationComplete& operation);

template <class ServerRPC, class RPCHandler, class CompletionHandlerT>
class RegisterRPCHandlerOperationAsioBase
    : public detail::RegisterRPCHandlerOperationBase<ServerRPC, RPCHandler,
                                                     detail::CancellationSlotT<CompletionHandlerT&>>,
      public detail::RegisterRPCHandlerOperationComplete,
      private detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandlerT>>
{
  public:
    using CompletionHandler = CompletionHandlerT;
    using StopToken = detail::CancellationSlotT<CompletionHandlerT&>;

  private:
    using Base = detail::RegisterRPCHandlerOperationBase<ServerRPC, RPCHandler, StopToken>;
    using CompletionBase = detail::RegisterRPCHandlerOperationComplete;
    using WorkTracker = detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandlerT>>;

    struct Decrementer
    {
        void operator()()
        {
            if (self_.decrement_ref_count())
            {
                detail::register_rpc_handler_asio_completion_trampoline(self_.grpc_context(), self_);
            }
        }

        RegisterRPCHandlerOperationAsioBase& self_;
    };

  public:
    using typename Base::ServerRPCExecutor;
    using typename Base::Service;
    using Executor = detail::AssociatedExecutorT<CompletionHandlerT, ServerRPCExecutor>;
    using Allocator = detail::AssociatedAllocatorT<CompletionHandlerT>;
    using RefCountGuard = detail::ScopeGuard<Decrementer>;

    template <class Ch>
    RegisterRPCHandlerOperationAsioBase(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler,
                                        Ch&& completion_handler, CompletionBase::Complete on_complete)
        : Base(executor, service, static_cast<RPCHandler&&>(rpc_handler)),
          CompletionBase(on_complete),
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
        const auto allocator = asio::get_associated_allocator(completion_handler);
        auto op = detail::allocate<
            Operation<ServerRPC, detail::RemoveCrefT<RPCHandler>, detail::RemoveCrefT<CompletionHandler>>>(
            allocator, executor, service_, static_cast<RPCHandler&&>(rpc_handler),
            static_cast<CompletionHandler&&>(completion_handler));
        (*op).initiate();
        op.release();
    }

    detail::ServerRPCServiceT<ServerRPC>& service_;
};

inline void register_rpc_handler_asio_completion_trampoline(agrpc::GrpcContext& grpc_context,
                                                            detail::RegisterRPCHandlerOperationComplete& operation)
{
    detail::ScopeGuard guard{[&operation]
                             {
                                 operation.complete();
                             }};
    agrpc::Alarm{grpc_context}.wait(detail::GrpcContextImplementation::TIME_ZERO,
                                    [g = std::move(guard)](auto&&...) mutable
                                    {
                                        g.execute();
                                    });
    grpc_context.work_finished();
}

template <class Operation>
void register_rpc_handler_asio_do_complete(detail::RegisterRPCHandlerOperationComplete& operation) noexcept
{
    auto& self = static_cast<Operation&>(operation);
    detail::AllocationGuard guard{self, self.get_allocator()};
    auto eptr{static_cast<std::exception_ptr&&>(self.error())};
    detail::dispatch_complete(guard, static_cast<std::exception_ptr&&>(eptr));
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_RPC_HANDLER_ASIO_BASE_HPP
