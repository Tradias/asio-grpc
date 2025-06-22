// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_UTILS_CLIENT_RPC_TEST_HPP
#define AGRPC_UTILS_CLIENT_RPC_TEST_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"
#include "utils/asio_utils.hpp"
#include "utils/client_context.hpp"
#include "utils/client_rpc.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_generic_client_server_test.hpp"
#include "utils/introspect_rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <doctest/doctest.h>

#include <functional>
#include <type_traits>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/register_callback_rpc_handler.hpp>
#include <agrpc/register_yield_rpc_handler.hpp>
#endif

namespace test
{
template <class ClientRPCT, class ServerRPCT = typename IntrospectRPC<ClientRPCT>::ServerRPC>
struct ClientServerRPCTest : std::conditional_t<(agrpc::ClientRPCType::GENERIC_UNARY == ClientRPCT::TYPE ||
                                                 agrpc::ClientRPCType::GENERIC_STREAMING == ClientRPCT::TYPE),
                                                GrpcGenericClientServerTest, GrpcClientServerTest>
{
    using ClientRPC = ClientRPCT;
    using ServerRPC = ServerRPCT;

    using Request = typename ClientRPC::Request;
    using Response = typename ClientRPC::Response;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class Executor, class... ClientFunctions>
    void spawn_client_functions(Executor&& executor, ClientFunctions&&... client_functions)
    {
        auto counter = std::make_shared<int>();
        test::spawn_many(
            executor,
            [counter, &client_functions, &server_shutdown = this->server_shutdown](const asio::yield_context& yield)
            {
                typename ClientRPC::Request request;
                typename ClientRPC::Response response;
                client_functions(request, response, yield);
                ++(*counter);
                if (*counter == sizeof...(client_functions))
                {
                    server_shutdown.initiate();
                }
            }...);
    }

    template <class RPCHandler, class... ClientFunctions>
    void register_callback_and_perform_requests(RPCHandler&& handler, ClientFunctions&&... client_functions)
    {
        agrpc::register_callback_rpc_handler<ServerRPC>(this->get_executor(), this->service, handler,
                                                        test::RethrowFirstArg{});
        spawn_client_functions(this->grpc_context, static_cast<ClientFunctions&&>(client_functions)...);
        this->grpc_context.run();
    }

    template <class RPCHandler, class ClientFunction>
    void register_callback_and_perform_three_requests(RPCHandler&& handler, ClientFunction&& client_function)
    {
        register_callback_and_perform_requests(static_cast<RPCHandler&&>(handler), client_function, client_function,
                                               client_function);
    }

    template <class RPCHandler, class... ClientFunctions>
    void register_and_perform_requests_no_shutdown(RPCHandler&& handler, ClientFunctions&&... client_functions)
    {
        agrpc::register_yield_rpc_handler<ServerRPC>(this->get_executor(), this->service, handler,
                                                     test::RethrowFirstArg{});
        test::spawn_and_run(this->grpc_context,
                            [&client_functions](const asio::yield_context& yield)
                            {
                                typename ClientRPC::Request request;
                                typename ClientRPC::Response response;
                                client_functions(request, response, yield);
                            }...);
    }

    template <class RPCHandler, class... ClientFunctions>
    void register_and_perform_requests(RPCHandler&& handler, ClientFunctions&&... client_functions)
    {
        agrpc::register_yield_rpc_handler<ServerRPC>(this->get_executor(), this->service, handler,
                                                     test::RethrowFirstArg{});
        spawn_client_functions(this->grpc_context, static_cast<ClientFunctions&&>(client_functions)...);
        this->grpc_context.run();
    }

    template <class RPCHandler, class ClientFunction>
    void register_and_perform_three_requests(RPCHandler&& handler, ClientFunction&& client_function)
    {
        register_and_perform_requests(static_cast<RPCHandler&&>(handler), client_function, client_function,
                                      client_function);
    }

    void run_server_immediate_cancellation(
        std::function<void(typename ClientRPC::Request&, typename ClientRPC::Response&, const asio::yield_context&)>
            client_func)
    {
        register_and_perform_three_requests([](auto&&...) {}, client_func);
    }
#endif

    auto create_rpc() { return ClientRPC{this->grpc_context, test::set_default_deadline}; }

    template <class CompletionToken>
    auto request_rpc(grpc::ClientContext& context, typename ClientRPC::Request& req, typename ClientRPC::Response& resp,
                     CompletionToken&& token)
    {
        return IntrospectRPC<ClientRPC>::request(this->grpc_context, *this->stub, context, req, resp, token);
    }

    template <class CompletionToken>
    auto request_rpc(bool use_executor, grpc::ClientContext& context, typename ClientRPC::Request& req,
                     typename ClientRPC::Response& resp, CompletionToken&& token)
    {
        if (use_executor)
        {
            return IntrospectRPC<ClientRPC>::request(this->get_executor(), *this->stub, context, req, resp, token);
        }
        return request_rpc(context, req, resp, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken>
    auto start_rpc(ClientRPC& rpc, typename ClientRPC::Request& req, typename ClientRPC::Response& resp,
                   CompletionToken&& token)
    {
        return IntrospectRPC<ClientRPC>::start(rpc, *this->stub, req, resp, token);
    }

    test::ServerShutdownInitiator server_shutdown{*this->server};
};
}

#endif  // AGRPC_UTILS_CLIENT_RPC_TEST_HPP
