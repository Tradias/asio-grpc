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

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/delete_guard.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/rpc.hpp"
#include "utils/test_server.hpp"
#include "utils/time.hpp"

#include <agrpc/high_level_client.hpp>
#include <agrpc/wait.hpp>

TYPE_TO_STRING(agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncUnary>);
TYPE_TO_STRING(agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming>);
TYPE_TO_STRING(agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming>);
TYPE_TO_STRING(agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming>);

using UnaryRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncUnary>;
using ClientStreamingRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncClientStreaming>;
using ServerStreamingRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming>;
using BidiStreamingRPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncBidirectionalStreaming>;

template <class RPC>
struct IntrospectRPC;

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>>
{
    static constexpr auto CLIENT_REQUEST = PrepareAsync;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestUnary;

    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>;

    template <class Stub, class CompletionToken>
    static auto request(agrpc::GrpcContext& grpc_context, Stub& stub, grpc::ClientContext& context,
                        const typename RPC::Request& request, const typename RPC::Response&, CompletionToken&& token)
    {
        return RPC::request(grpc_context, stub, context, request, token);
    }
};

template <auto PrepareAsync, class Executor>
struct IntrospectRPC<agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>>
{
    static constexpr auto CLIENT_REQUEST = PrepareAsync;
    static constexpr auto SERVER_REQUEST = &test::v1::Test::AsyncService::RequestServerStreaming;

    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;

    template <class Stub, class CompletionToken>
    static auto request(agrpc::GrpcContext& grpc_context, Stub& stub, grpc::ClientContext& context,
                        const typename RPC::Request& request, typename RPC::Response&, CompletionToken&& token)
    {
        return RPC::request(grpc_context, stub, context, request, token);
    }
};

template <class RPC>
struct HighLevelClientTest : test::GrpcClientServerTest
{
    static constexpr auto SERVER_REQUEST = IntrospectRPC<RPC>::SERVER_REQUEST;

    template <class... Functions>
    void spawn_and_run(Functions&&... functions)
    {
        (asio::spawn(grpc_context, std::forward<Functions>(functions)), ...);
        grpc_context.run();
    }

    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return IntrospectRPC<RPC>::request(grpc_context, *stub, client_context, request, response, token);
    }

    typename RPC::Request request;
    typename RPC::Response response;
    test::TestServer<SERVER_REQUEST> test_server{service, server_context};
};

TEST_CASE_TEMPLATE("RPC::read_initial_metadata automatically finishes RPC on error", RPC, ServerStreamingRPC)
{
    HighLevelClientTest<RPC> test;
    test.spawn_and_run(
        [&](asio::yield_context yield)
        {
            CHECK(test.test_server.request_rpc(yield));
        },
        [&](asio::yield_context yield)
        {
            auto rpc = test.request_rpc(yield);
            CHECK(rpc.ok());
            test.client_context.TryCancel();
            CHECK_FALSE(rpc.read_initial_metadata(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.error_code());
        });
}

TEST_CASE_FIXTURE(HighLevelClientTest<UnaryRPC>, "RPC::request can have UseSender as default completion token")
{
    using RPC = agrpc::UseSender::as_default_on_t<agrpc::BasicRPC<&test::v1::Test::Stub::PrepareAsyncUnary>>;
    bool ok{};
    test::DeleteGuard guard{};
    bool use_submit{};
    SUBCASE("submit") { use_submit = true; }
    SUBCASE("start") {}
    spawn_and_run(
        [&](asio::yield_context yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK_EQ(42, test_server.request.integer());
            test_server.response.set_integer(21);
            CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
        },
        [&](auto&&)
        {
            request.set_integer(42);
            auto sender = RPC::request(grpc_context, *stub, client_context, request, response);
            const auto receiver = test::FunctionAsReceiver{[&](RPC&& rpc)
                                                           {
                                                               ok = rpc.ok();
                                                           }};
            if (use_submit)
            {
                CHECK_FALSE(asio::execution::can_submit_v<std::add_const_t<decltype(sender)>, decltype(receiver)>);
                asio::execution::submit(std::move(sender), receiver);
            }
            else
            {
                CHECK_FALSE(asio::execution::can_connect_v<std::add_const_t<decltype(sender)>, decltype(receiver)>);
                auto& operation_state = guard.emplace_with(
                    [&]
                    {
                        return asio::execution::connect(std::move(sender), receiver);
                    });
                asio::execution::start(operation_state);
            }
        });
    CHECK(ok);
    CHECK_EQ(21, response.integer());
}
