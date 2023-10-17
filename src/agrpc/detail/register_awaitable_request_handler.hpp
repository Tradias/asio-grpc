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

#ifndef AGRPC_DETAIL_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP
#define AGRPC_DETAIL_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP

#include <agrpc/bind_allocator.hpp>
#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
#include <agrpc/detail/register_request_handler_asio_base.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ServerRPC, class RequestHandler, class CompletionHandler>
struct AwaitableRequestHandlerOperation
    : detail::RegisterRequestHandlerOperationAsioBase<ServerRPC, RequestHandler, CompletionHandler>
{
    using Base = detail::RegisterRequestHandlerOperationAsioBase<ServerRPC, RequestHandler, CompletionHandler>;
    using typename Base::Allocator;
    using typename Base::RefCountGuard;
    using typename Base::RPCRequest;
    using typename Base::ServerRPCExecutor;
    using typename Base::Service;

    using Awaitable = detail::RebindCoroutineT<decltype(std::declval<RPCRequest&>().invoke(
                                                   std::declval<RequestHandler&>(), std::declval<ServerRPC&>())),
                                               void>;
    using UseAwaitable = detail::CoroutineCompletionTokenT<Awaitable>;

    template <class Ch>
    AwaitableRequestHandlerOperation(const ServerRPCExecutor& executor, Service& service,
                                     RequestHandler&& request_handler, Ch&& completion_handler)
        : Base(executor, service, static_cast<RequestHandler&&>(request_handler), static_cast<Ch&&>(completion_handler),
               &detail::register_request_handler_asio_do_complete<AwaitableRequestHandlerOperation>)
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
                               auto& self = static_cast<AwaitableRequestHandlerOperation&>(g.get().self_);
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
        RPCRequest req;
        if (!co_await req.start(rpc, this->service(), use_awaitable()))
        {
            co_return;
        }
        initiate_next();
        AGRPC_TRY { co_await req.invoke(this->request_handler(), rpc); }
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
            return agrpc::AllocatorBinder(this->get_allocator(), UseAwaitable{});
        }
    }
};

template <class ServerRPC>
using RegisterAwaitableRequestHandlerInitiator =
    detail::RegisterRequestHandlerInitiator<ServerRPC, AwaitableRequestHandlerOperation>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP
