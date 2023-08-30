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

#ifndef AGRPC_AGRPC_REGISTER_SPAWN_HANDLER_HPP
#define AGRPC_AGRPC_REGISTER_SPAWN_HANDLER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/coroutine_traits.hpp>
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
}

template <class ServerRPC, class Service, class RequestHandler, class Executor>
void register_yield_handler(const typename ServerRPC::executor_type& executor, Service& service,
                            RequestHandler request_handler, const asio::basic_yield_context<Executor>& yield)
{
    static constexpr bool HAS_INITIAL_REQUEST =
        ServerRPC::TYPE == agrpc::ServerRPCType::SERVER_STREAMING || ServerRPC::TYPE == agrpc::ServerRPCType::UNARY;
    auto rpc = detail::ServerRPCContextBaseAccess::construct<ServerRPC>(executor);
    detail::RPCRequest<typename ServerRPC::Request, HAS_INITIAL_REQUEST> req;
    if (!req.start(rpc, service, yield))
    {
        return;
    }
    detail::spawn(yield,
                  [executor, &service, request_handler](const asio::basic_yield_context<Executor>& yield) mutable
                  {
                      agrpc::register_yield_handler<ServerRPC>(executor, service,
                                                               static_cast<RequestHandler&&>(request_handler), yield);
                  });
    std::exception_ptr eptr;
    AGRPC_TRY { req.invoke(static_cast<RequestHandler&&>(request_handler), rpc, yield); }
    AGRPC_CATCH(...) { eptr = std::current_exception(); }
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
    if constexpr (ServerRPC::Traits::RESUMABLE_READ)
    {
        if (rpc.is_reading())
        {
            rpc.wait_for_read(yield);
        }
    }
    if (eptr)
    {
        std::rethrow_exception(eptr);
    }
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REGISTER_SPAWN_HANDLER_HPP
