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

#ifndef AGRPC_DETAIL_REGISTER_YIELD_RPC_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_YIELD_RPC_HANDLER_HPP

#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/register_rpc_handler_asio_base.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>
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

template <class ServerRPC, class RPCHandler, class CompletionHandler>
struct RegisterYieldRPCHandlerOperation
    : detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>
{
    using Base = detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>;
    using typename Base::Allocator;
    using typename Base::RefCountGuard;
    using typename Base::RPCRequest;
    using typename Base::ServerRPCExecutor;
    using typename Base::Service;

    template <class Ch>
    RegisterYieldRPCHandlerOperation(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler,
                                     Ch&& completion_handler)
        : Base(executor, service, static_cast<RPCHandler&&>(rpc_handler), static_cast<Ch&&>(completion_handler),
               &detail::register_rpc_handler_asio_do_complete<RegisterYieldRPCHandlerOperation>)
    {
        initiate();
    }

    void initiate()
    {
        this->increment_ref_count();
        detail::spawn(
            asio::get_associated_executor(this->completion_handler(), this->get_executor()),
            [g = RefCountGuard{*this}](const auto& yield)
            {
                static_cast<RegisterYieldRPCHandlerOperation&>(g.get().self_).perform_request_and_repeat(yield);
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
        auto rpc = detail::ServerRPCContextBaseAccess::construct<ServerRPC>(this->get_executor());
        RPCRequest req;
        if (!req.start(rpc, this->service(), use_yield(yield)))
        {
            return;
        }
        initiate_next();
        AGRPC_TRY { req.invoke(this->rpc_handler(), rpc, yield); }
        AGRPC_CATCH(...) { this->set_error(std::current_exception()); }
        if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
        {
            rpc.cancel();
        }
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (!rpc.is_done())
            {
                rpc.wait_for_done(use_yield(yield));
            }
        }
    }

    template <class Yield>
    decltype(auto) use_yield(const Yield& yield)
    {
        if constexpr (detail::IS_STD_ALLOCATOR<Allocator>)
        {
            return (yield);
        }
        else
        {
            return detail::AllocatorBinder(this->get_allocator(), yield);
        }
    }
};

template <class ServerRPC>
using RegisterYieldRPCHandlerInitiator =
    detail::RegisterRPCHandlerInitiator<ServerRPC, RegisterYieldRPCHandlerOperation>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_YIELD_RPC_HANDLER_HPP
