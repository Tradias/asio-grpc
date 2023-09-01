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
#include "utils/test_server.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <doctest/doctest.h>

#include <functional>

namespace test
{
template <class RPC>
struct ClientRPCTest : std::conditional_t<(agrpc::ClientRPCType::GENERIC_UNARY == RPC::TYPE ||
                                           agrpc::ClientRPCType::GENERIC_STREAMING == RPC::TYPE),
                                          test::GrpcGenericClientServerTest, test::GrpcClientServerTest>
{
    static constexpr auto SERVER_REQUEST = test::IntrospectRPC<RPC>::SERVER_REQUEST;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class... Functions>
    void spawn_and_run(Functions&&... functions)
    {
        test::spawn_and_run(this->grpc_context, std::forward<Functions>(functions)...);
    }
#endif

    auto create_rpc() { return RPC{this->grpc_context, test::set_default_deadline}; }

    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::request(this->grpc_context, *this->stub, this->client_context, request, response,
                                           token);
    }

    template <class CompletionToken>
    auto request_rpc(grpc::ClientContext& context, typename RPC::Request& req, typename RPC::Response& resp,
                     CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::request(this->grpc_context, *this->stub, context, req, resp, token);
    }

    template <class CompletionToken>
    auto request_rpc(bool use_executor, CompletionToken&& token)
    {
        return request_rpc(use_executor, this->client_context, request, response,
                           static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken>
    auto request_rpc(bool use_executor, grpc::ClientContext& context, typename RPC::Request& req,
                     typename RPC::Response& resp, CompletionToken&& token)
    {
        if (use_executor)
        {
            return IntrospectRPC<RPC>::request(this->get_executor(), *this->stub, context, req, resp, token);
        }
        return request_rpc(context, req, resp, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken>
    auto start_rpc(RPC& rpc, CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::start(rpc, *this->stub, request, response, token);
    }

    template <class CompletionToken>
    auto start_rpc(RPC& rpc, typename RPC::Request& req, typename RPC::Response& resp, CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::start(rpc, *this->stub, req, resp, token);
    }

    template <class CompletionToken>
    void server_request_rpc_and_cancel(CompletionToken&& token)
    {
        if (test_server.request_rpc(token))
        {
            this->server_context.TryCancel();
        }
    }

    typename RPC::Request request;
    typename RPC::Response response;
    test::TestServer<SERVER_REQUEST> test_server{this->service, this->server_context};
    test::ServerShutdownInitiator server_shutdown{*this->server};
};
}

#endif  // AGRPC_UTILS_CLIENT_RPC_TEST_HPP
