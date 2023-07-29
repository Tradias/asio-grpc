// Copyright 2022 Dennis Hezel
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

#ifndef AGRPC_UTILS_CLIENT_RPC_HPP
#define AGRPC_UTILS_CLIENT_RPC_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"
#include "utils/asio_utils.hpp"
#include "utils/client_context.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/test_server.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <doctest/doctest.h>

#include <functional>

namespace test
{
using UnaryClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncUnary>;
using UnaryInterfaceClientRPC = agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncUnary>;
using ClientStreamingClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming>;
using ClientStreamingInterfaceClientRPC = agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncClientStreaming>;
using ServerStreamingClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming>;
using ServerStreamingInterfaceClientRPC = agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncServerStreaming>;
using BidirectionalStreamingClientRPC = agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming>;
using BidirectionalStreamingInterfaceClientRPC =
    agrpc::ClientRPC<&test::v1::Test::StubInterface::PrepareAsyncBidirectionalStreaming>;
using GenericUnaryClientRPC = agrpc::ClientRPCGenericUnary<>;
using GenericStreamingClientRPC = agrpc::ClientRPCGenericStreaming<>;

template <class RPC, auto Type = RPC::TYPE>
struct IntrospectRPC;

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::UNARY>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::GenericStub&,
                        grpc::ClientContext& context, const typename RPC::Request& request,
                        typename RPC::Response& response, CompletionToken&& token)
    {
        return RPC::request(executor, stub, context, request, response, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::ClientRPCGenericUnary<Executor>, agrpc::ClientRPCType::GENERIC_UNARY>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using RPC = agrpc::ClientRPCGenericUnary<Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, test::v1::Test::Stub&, typename RPC::Stub& stub,
                        grpc::ClientContext& context, const typename RPC::Request& request,
                        typename RPC::Response& response, CompletionToken&& token)
    {
        return RPC::request(executor, "/test.v1.Test/Unary", stub, context, request, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::CLIENT_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestClientStreaming;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, grpc::GenericStub&, const typename RPC::Request&,
                      typename RPC::Response& response, CompletionToken&& token)
    {
        return rpc.start(stub, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::SERVER_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestServerStreaming;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, grpc::GenericStub&, const typename RPC::Request& request,
                      const typename RPC::Response&, CompletionToken&& token)
    {
        return rpc.start(stub, request, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::ClientRPC<PrepareAsync, Executor>, agrpc::ClientRPCType::BIDIRECTIONAL_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestBidirectionalStreaming;

    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, typename RPC::Stub& stub, grpc::GenericStub&, const typename RPC::Request&,
                      const typename RPC::Response&, CompletionToken&& token)
    {
        return rpc.start(stub, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::ClientRPCGenericStreaming<Executor>, agrpc::ClientRPCType::GENERIC_STREAMING>
{
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestBidirectionalStreaming;

    using RPC = agrpc::ClientRPCGenericStreaming<Executor>;

    template <class CompletionToken>
    static auto start(RPC& rpc, test::v1::Test::Stub&, typename RPC::Stub& stub, const typename RPC::Request&,
                      const typename RPC::Response&, CompletionToken&& token)
    {
        return rpc.start("/test.v1.Test/BidirectionalStreaming", stub, token);
    }
};

template <class RPC>
struct ClientRPCTest : test::GrpcClientServerTest
{
    static constexpr auto SERVER_REQUEST = IntrospectRPC<RPC>::SERVER_REQUEST;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class... Functions>
    void spawn_and_run(Functions&&... functions)
    {
        test::spawn_and_run(grpc_context, std::forward<Functions>(functions)...);
    }
#endif

    auto create_rpc() { return RPC{grpc_context, test::set_default_deadline}; }

    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::request(grpc_context, *stub, generic_stub, client_context, request, response, token);
    }

    template <class CompletionToken>
    auto request_rpc(bool use_executor, CompletionToken&& token)
    {
        if (use_executor)
        {
            return IntrospectRPC<RPC>::request(this->get_executor(), *stub, generic_stub, client_context, request,
                                               response, token);
        }
        return request_rpc(token);
    }

    template <class CompletionToken>
    auto start_rpc(RPC& rpc, CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::start(rpc, *stub, generic_stub, request, response, token);
    }

    template <class CompletionToken>
    void server_request_rpc_and_cancel(CompletionToken&& token)
    {
        if (test_server.request_rpc(token))
        {
            server_context.TryCancel();
        }
    }

    typename RPC::Request request;
    typename RPC::Response response;
    test::TestServer<SERVER_REQUEST> test_server{service, server_context};
    test::ServerShutdownInitiator server_shutdown{*server};
    grpc::GenericStub generic_stub{channel};
};
}

TYPE_TO_STRING(test::UnaryClientRPC);
TYPE_TO_STRING(test::UnaryInterfaceClientRPC);
TYPE_TO_STRING(test::ClientStreamingClientRPC);
TYPE_TO_STRING(test::ClientStreamingInterfaceClientRPC);
TYPE_TO_STRING(test::ServerStreamingClientRPC);
TYPE_TO_STRING(test::ServerStreamingInterfaceClientRPC);
TYPE_TO_STRING(test::BidirectionalStreamingClientRPC);
TYPE_TO_STRING(test::BidirectionalStreamingInterfaceClientRPC);
TYPE_TO_STRING(test::GenericUnaryClientRPC);
TYPE_TO_STRING(test::GenericStreamingClientRPC);

#endif  // AGRPC_UTILS_CLIENT_RPC_HPP
