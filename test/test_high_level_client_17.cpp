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
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/rpc.hpp"
#include "utils/time.hpp"

#include <agrpc/high_level_client.hpp>
#include <agrpc/wait.hpp>

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "RPC::read_initial_metadata automatically finishes RPC on error")
{
    test::spawn_and_run(
        grpc_context,
        [&](asio::yield_context yield)
        {
            test::msg::Request request;
            grpc::ServerAsyncWriter<test::msg::Response> writer{&server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, service, server_context,
                                 request, writer, yield));
        },
        [&](asio::yield_context yield)
        {
            using RPC = agrpc::RPC<&test::v1::Test::Stub::PrepareAsyncServerStreaming>;
            RPC::Request request;
            auto rpc = RPC::request(grpc_context, *stub, client_context, request, yield);
            CHECK(rpc.ok());
            client_context.TryCancel();
            CHECK_FALSE(rpc.read_initial_metadata(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.error_code());
        });
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest,
                  "RPC::read_initial_metadata can have UseSender as default completion token")
{
    using RPC = agrpc::UseSender::as_default_on_t<
        agrpc::BasicRPC<&test::v1::Test::Stub::PrepareAsyncUnary, agrpc::GrpcExecutor>>;
    bool ok{};
    RPC::Response response;
    bool use_submit{};
    SUBCASE("submit") { use_submit = true; }
    SUBCASE("yield") {}
    test::spawn_and_run(
        grpc_context,
        [&](asio::yield_context yield)
        {
            test::msg::Request request;
            grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request, writer,
                                 yield));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
        },
        [&](auto&& yield)
        {
            RPC::Request request;
            request.set_integer(42);
            if (use_submit)
            {
                auto sender = RPC::request(grpc_context, *stub, client_context, request, response);
                asio::execution::submit(std::move(sender), test::FunctionAsReceiver{[&](RPC&& rpc)
                                                                                    {
                                                                                        ok = rpc.ok();
                                                                                    }});
            }
            else
            {
                auto rpc = RPC::request(grpc_context, *stub, client_context, request, response, yield);
                ok = rpc.ok();
            }
        });
    CHECK(ok);
    CHECK_EQ(21, response.integer());
}
