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

#ifndef AGRPC_UTILS_HIGH_LEVEL_CLIENT_HPP
#define AGRPC_UTILS_HIGH_LEVEL_CLIENT_HPP

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_forward.hpp"
#include "utils/asio_utils.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/test_server.hpp"

#include <agrpc/high_level_client.hpp>
#include <doctest/doctest.h>

#include <functional>

namespace test
{
using UnaryRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncUnary>;
using ClientStreamingRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming>;
using ServerStreamingRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming>;
using BidirectionalStreamingRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming>;
using GenericUnaryRPC = agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC>;
using GenericStreamingRPC = agrpc::RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC>;

template <class RPC>
struct IntrospectRPC;

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>>
{
    static constexpr auto CLIENT_REQUEST = PrepareAsync;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using RPC = agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::GenericStub&,
                        grpc::ClientContext& context, const typename RPC::Request& request,
                        typename RPC::Response& response, CompletionToken&& token)
    {
        return RPC::request(executor, stub, context, request, response, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC, Executor>>
{
    static constexpr auto CLIENT_REQUEST = agrpc::CLIENT_GENERIC_UNARY_RPC;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using RPC = agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC, Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, test::v1::Test::Stub&, typename RPC::Stub& stub,
                        grpc::ClientContext& context, const typename RPC::Request& request,
                        typename RPC::Response& response, CompletionToken&& token)
    {
        return RPC::request(executor, "/test.v1.Test/Unary", stub, context, request, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>>
{
    static constexpr auto CLIENT_REQUEST = PrepareAsync;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestClientStreaming;

    using RPC = agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::GenericStub&,
                        grpc::ClientContext& context, const typename RPC::Request&, typename RPC::Response& response,
                        CompletionToken&& token)
    {
        return RPC::request(executor, stub, context, response, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>>
{
    static constexpr auto CLIENT_REQUEST = PrepareAsync;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestServerStreaming;

    using RPC = agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::GenericStub&,
                        grpc::ClientContext& context, const typename RPC::Request& request,
                        const typename RPC::Response&, CompletionToken&& token)
    {
        return RPC::request(executor, stub, context, request, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>>
{
    static constexpr auto CLIENT_REQUEST = PrepareAsync;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestBidirectionalStreaming;

    using RPC = agrpc::RPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, typename RPC::Stub& stub, grpc::GenericStub&,
                        grpc::ClientContext& context, const typename RPC::Request&, const typename RPC::Response&,
                        CompletionToken&& token)
    {
        return RPC::request(executor, stub, context, token);
    }
};

template <class Executor>
struct IntrospectRPC<agrpc::RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor>>
{
    static constexpr auto CLIENT_REQUEST = agrpc::CLIENT_GENERIC_STREAMING_RPC;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestBidirectionalStreaming;

    using RPC = agrpc::RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor>;

    template <class ExecOrContext, class CompletionToken>
    static auto request(ExecOrContext&& executor, test::v1::Test::Stub&, typename RPC::Stub& stub,
                        grpc::ClientContext& context, const typename RPC::Request&, const typename RPC::Response&,
                        CompletionToken&& token)
    {
        return RPC::request(executor, "/test.v1.Test/BidirectionalStreaming", stub, context, token);
    }
};

template <class RPC>
struct HighLevelClientTest : test::GrpcClientServerTest
{
    static constexpr auto SERVER_REQUEST = IntrospectRPC<RPC>::SERVER_REQUEST;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class... Functions>
    void spawn_and_run(Functions&&... functions)
    {
        test::spawn_and_run(grpc_context, std::forward<Functions>(functions)...);
    }
#endif

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
        else
        {
            return request_rpc(token);
        }
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

TYPE_TO_STRING(test::UnaryRPC);
TYPE_TO_STRING(test::ClientStreamingRPC);
TYPE_TO_STRING(test::ServerStreamingRPC);
TYPE_TO_STRING(test::BidirectionalStreamingRPC);

#endif  // AGRPC_UTILS_HIGH_LEVEL_CLIENT_HPP
