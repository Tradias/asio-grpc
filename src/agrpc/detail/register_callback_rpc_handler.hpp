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

#ifndef AGRPC_DETAIL_REGISTER_CALLBACK_RPC_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_CALLBACK_RPC_HANDLER_HPP

#include <agrpc/detail/register_rpc_handler_asio_base.hpp>
#include <agrpc/detail/server_rpc_with_request.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/server_rpc_ptr.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ServerRPC, class RPCHandler, class CompletionHandler>
struct RegisterCallbackRPCHandlerOperation
    : detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>
{
    using Base = detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>;
    using typename Base::Allocator;
    using typename Base::RefCountGuard;
    using typename Base::ServerRPCExecutor;
    using typename Base::Service;
    using ServerRPCWithRequest = detail::ServerRPCWithRequest<ServerRPC>;
    using ServerRPCPtr = agrpc::ServerRPCPtr<ServerRPC>;
    using Starter = detail::ServerRPCStarter<>;

    struct ServerRPCAllocation : detail::ServerRPCPtrRequestMessageFactoryT<ServerRPC, RPCHandler>
    {
        ServerRPCAllocation(const ServerRPCExecutor& executor, RegisterCallbackRPCHandlerOperation& self)
            : detail::ServerRPCPtrRequestMessageFactoryT<ServerRPC, RPCHandler>(self.rpc_handler(), executor),
              self_(self)
        {
        }

        RegisterCallbackRPCHandlerOperation& self_;
    };

    struct StartCallback
    {
        using allocator_type = Allocator;

        void operator()(bool ok)
        {
            if (ok)
            {
                self_.notify_when_done_work_started();
                AGRPC_TRY
                {
                    self_.initiate_next();
                    auto& rpc = *static_cast<ServerRPCAllocation*>(ptr_.server_rpc_);
                    Starter::invoke(self_.rpc_handler(), static_cast<ServerRPCPtr&&>(ptr_), rpc);
                }
                AGRPC_CATCH(...)
                {
                    // Technically `this` could already be deallocated at this point but we rely on the
                    // fact that completing this operation is done in a manner similar to asio::post and
                    // therefore never before this lambda ends.
                    self_.set_error(std::current_exception());
                }
            }
            else
            {
                [[maybe_unused]] RefCountGuard a{self_};
                [[maybe_unused]] detail::AllocationGuard b{*static_cast<ServerRPCAllocation*>(ptr_.release()),
                                                           self_.get_allocator()};
            }
        }

        Allocator get_allocator() const noexcept { return self_.get_allocator(); }

        RegisterCallbackRPCHandlerOperation& self_;
        ServerRPCPtr ptr_;
    };

    struct WaitForDoneCallback
    {
        using allocator_type = Allocator;

        void operator()(const detail::ErrorCode&) const noexcept {}

        Allocator get_allocator() const noexcept { return self_.get_allocator(); }

        RegisterCallbackRPCHandlerOperation& self_;
        ServerRPCPtr ptr_;
    };

    static void wait_for_done_deleter(ServerRPCWithRequest* ptr) noexcept
    {
        auto& allocation = *static_cast<ServerRPCAllocation*>(ptr);
        [[maybe_unused]] RefCountGuard a{allocation.self_};
        [[maybe_unused]] detail::AllocationGuard b{allocation, allocation.self_.get_allocator()};
    }

    static void deleter(ServerRPCWithRequest* ptr) noexcept
    {
        auto& allocation = *static_cast<ServerRPCAllocation*>(ptr);
        auto& self = allocation.self_;
        RefCountGuard ref_count_guard{self};
        detail::AllocationGuard alloc_guard{allocation, self.get_allocator()};
        auto& rpc = ptr->rpc_;
        if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
        {
            rpc.cancel();
        }
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (!rpc.is_done())
            {
                rpc.wait_for_done(WaitForDoneCallback{self, ServerRPCPtr{ptr, &wait_for_done_deleter}});
                ref_count_guard.release();
                alloc_guard.release();
            }
        }
    }

    template <class Ch>
    RegisterCallbackRPCHandlerOperation(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler,
                                        Ch&& completion_handler)
        : Base(executor, service, static_cast<RPCHandler&&>(rpc_handler), static_cast<Ch&&>(completion_handler),
               &detail::register_rpc_handler_asio_do_complete<RegisterCallbackRPCHandlerOperation>)
    {
    }

    void initiate()
    {
        auto ptr = detail::allocate<ServerRPCAllocation>(this->get_allocator(), this->get_executor(), *this);
        this->increment_ref_count();
        perform_request_and_repeat({ptr.extract(), &deleter});
    }

    void initiate_next()
    {
        if AGRPC_LIKELY (!this->is_stopped())
        {
            initiate();
        }
    }

    void perform_request_and_repeat(ServerRPCPtr&& ptr)
    {
        auto& rpc = *static_cast<ServerRPCAllocation*>(ptr.server_rpc_);
        Starter::start(rpc.rpc_, this->service(), rpc, StartCallback{*this, static_cast<ServerRPCPtr&&>(ptr)});
    }
};

template <class ServerRPC>
using RegisterCallbackRPCHandlerInitiator =
    detail::RegisterRPCHandlerInitiator<ServerRPC, RegisterCallbackRPCHandlerOperation>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_CALLBACK_RPC_HANDLER_HPP
