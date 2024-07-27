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

#ifndef AGRPC_DETAIL_REGISTER_AWAITABLE_RPC_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_AWAITABLE_RPC_HANDLER_HPP

#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
#include <agrpc/detail/register_rpc_handler_asio_base.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/awaitable.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ServerRPC, class RPCHandler, class CompletionHandler>
struct RegisterAwaitableRPCHandlerOperation
    : detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>
{
    using Base = detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>;
    using typename Base::Allocator;
    using typename Base::RefCountGuard;
    using typename Base::ServerRPCExecutor;
    using typename Base::Service;
    using typename Base::Starter;

    using Awaitable =
        detail::RebindCoroutineT<detail::RPCHandlerInvokeResultT<Starter&, RPCHandler&, ServerRPC&>, void>;
    using UseAwaitable = detail::CoroutineCompletionTokenT<Awaitable>;

    template <class Ch>
    RegisterAwaitableRPCHandlerOperation(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler,
                                         Ch&& completion_handler)
        : Base(executor, service, static_cast<RPCHandler&&>(rpc_handler), static_cast<Ch&&>(completion_handler),
               &detail::register_rpc_handler_asio_do_complete<RegisterAwaitableRPCHandlerOperation>)
    {
        initiate();
    }

    void initiate()
    {
        this->increment_ref_count();
        asio::co_spawn(asio::get_associated_executor(this->completion_handler(), this->get_executor()),
                       perform_request_and_repeat(),
                       [g = RefCountGuard{*this}](std::exception_ptr eptr)
                       {
                           if (eptr)
                           {
                               auto& self = static_cast<RegisterAwaitableRPCHandlerOperation&>(g.get().self_);
                               self.set_error(static_cast<std::exception_ptr&&>(eptr));
                           }
                       });
    }

    void initiate_next()
    {
        if AGRPC_LIKELY (!this->is_stopped())
        {
            initiate();
        }
    }

    Awaitable perform_request_and_repeat()
    {
        auto rpc = detail::ServerRPCContextBaseAccess::construct<ServerRPC>(this->get_executor());
        Starter starter;
        if (!co_await starter.start(rpc, this->service(), use_awaitable()))
        {
            co_return;
        }
        initiate_next();
        AGRPC_TRY { co_await starter.invoke(this->rpc_handler(), rpc); }
        AGRPC_CATCH(...) { this->set_error(std::current_exception()); }
        if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
        {
            rpc.cancel();
        }
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (!rpc.is_done())
            {
                co_await rpc.wait_for_done(use_awaitable());
            }
        }
    }

    constexpr auto use_awaitable()
    {
        if constexpr (detail::IS_STD_ALLOCATOR<Allocator>)
        {
            return UseAwaitable{};
        }
        else
        {
            return detail::AllocatorBinder(this->get_allocator(), UseAwaitable{});
        }
    }
};

template <class ServerRPC>
using RegisterAwaitableRPCHandlerInitiator =
    detail::RegisterRPCHandlerInitiator<ServerRPC, RegisterAwaitableRPCHandlerOperation>;
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_REGISTER_AWAITABLE_RPC_HANDLER_HPP
