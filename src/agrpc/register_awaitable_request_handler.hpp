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

#ifndef AGRPC_AGRPC_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/detail/start_server_rpc.hpp>
#include <agrpc/grpc_context.hpp>

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/co_spawn.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/co_spawn.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

template <class ServerRPC, class Service, class RequestHandler, class Executor = asio::any_io_executor>
asio::awaitable<void, Executor> register_awaitable_request_handler(typename ServerRPC::executor_type executor,
                                                                   Service& service, RequestHandler request_handler)
{
    auto rpc = detail::ServerRPCContextBaseAccess::construct<ServerRPC>(executor);
    detail::RPCRequest<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)> req;
    if (!co_await req.start(rpc, service, asio::use_awaitable_t<Executor>{}))
    {
        co_return;
    }
    asio::co_spawn(co_await asio::this_coro::executor,
                   agrpc::register_awaitable_request_handler<ServerRPC>(executor, service, request_handler),
                   detail::RethrowFirstArg{});
    std::exception_ptr eptr;
    AGRPC_TRY { co_await req.invoke(static_cast<RequestHandler&&>(request_handler), rpc); }
    AGRPC_CATCH(...) { eptr = std::current_exception(); }
    if (!detail::ServerRPCContextBaseAccess::is_finished(rpc))
    {
        rpc.cancel();
    }
    if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
    {
        if (!rpc.is_done())
        {
            co_await rpc.wait_for_done(asio::use_awaitable_t<Executor>{});
        }
    }
    if (eptr)
    {
        std::rethrow_exception(eptr);
    }
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REGISTER_AWAITABLE_REQUEST_HANDLER_HPP
