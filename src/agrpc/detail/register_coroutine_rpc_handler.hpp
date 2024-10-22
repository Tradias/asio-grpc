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

#ifndef AGRPC_DETAIL_REGISTER_COROUTINE_RPC_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_COROUTINE_RPC_HANDLER_HPP

#include <agrpc/detail/awaitable.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/register_rpc_handler_asio_base.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class CoroTraits>
struct RegisterCoroutineRPCHandlerOperation
{
    template <class ServerRPC, class RPCHandler, class CompletionHandler>
    struct Type : detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>
    {
        using Base = detail::RegisterRPCHandlerOperationAsioBase<ServerRPC, RPCHandler, CompletionHandler>;
        using typename Base::Allocator;
        using typename Base::RefCountGuard;
        using typename Base::ServerRPCExecutor;
        using typename Base::Service;

        using Awaitable = typename CoroTraits::template Rebind<void>;

        template <class Ch>
        Type(const ServerRPCExecutor& executor, Service& service, RPCHandler&& rpc_handler, Ch&& completion_handler)
            : Base(executor, service, static_cast<RPCHandler&&>(rpc_handler), static_cast<Ch&&>(completion_handler),
                   &detail::register_rpc_handler_asio_do_complete<Type>)
        {
            initiate();
        }

        void initiate()
        {
            this->increment_ref_count();
            CoroTraits::co_spawn(this->get_executor(), this->rpc_handler(), this->completion_handler(),
                                 [g = RefCountGuard{*this}](auto&&... args) mutable -> Awaitable
                                 {
                                     return perform_request_and_repeat(std::move(g),
                                                                       static_cast<decltype(args)&&>(args)...);
                                 });
        }

        void initiate_next()
        {
            if AGRPC_LIKELY (!this->is_stopped())
            {
                initiate();
            }
        }

        template <class... Args>
        static Awaitable perform_request_and_repeat(RefCountGuard g, Args... args)
        {
            auto& self = static_cast<Type&>(g.get().self_);
            auto rpc = detail::ServerRPCContextBaseAccess::construct<ServerRPC>(self.get_executor());
            detail::ServerRPCStarterT<ServerRPC, Args...> starter;
            if (!co_await starter.start(rpc, self.service(), self.completion_token()))
            {
                co_return;
            }
            self.notify_when_done_work_started();
            AGRPC_TRY
            {
                self.initiate_next();
                co_await starter.invoke(self.rpc_handler(), static_cast<Args&&>(args)..., rpc);
            }
            AGRPC_CATCH(...) { self.set_error(std::current_exception()); }
            if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
            {
                rpc.cancel();
            }
            if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
            {
                if (!rpc.is_done())
                {
                    co_await rpc.wait_for_done(self.completion_token());
                }
            }
        }

        auto completion_token()
        {
            if constexpr (detail::IS_STD_ALLOCATOR<Allocator>)
            {
                return CoroTraits::completion_token(this->rpc_handler(), this->completion_handler());
            }
            else
            {
                return detail::AllocatorBinder(
                    this->get_allocator(),
                    CoroTraits::completion_token(this->rpc_handler(), this->completion_handler()));
            }
        }
    };
};

template <class ServerRPC, class CoroTraits>
using RegisterCoroutineRPCHandlerInitiator =
    detail::RegisterRPCHandlerInitiator<ServerRPC, RegisterCoroutineRPCHandlerOperation<CoroTraits>::template Type>;
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_REGISTER_COROUTINE_RPC_HANDLER_HPP
