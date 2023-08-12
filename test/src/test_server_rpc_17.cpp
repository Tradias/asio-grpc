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
#include "utils/client_rpc.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_generic_client_server_test.hpp"
#include "utils/introspect_rpc.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"
#include "utils/server_rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/test_server.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <agrpc/register_yield_handler.hpp>
#include <agrpc/server_rpc.hpp>

#include <cstddef>

template <class RPC>
struct ServerRPCTest : std::conditional_t<std::is_same_v<agrpc::GenericServerRPC<>, RPC>,
                                          test::GrpcGenericClientServerTest, test::GrpcClientServerTest>
{
    static constexpr auto SERVER_REQUEST = test::IntrospectRPC<RPC>::SERVER_REQUEST;

    using ServerRPC = RPC;
    using ClientRPC = typename test::IntrospectRPC<RPC>::ClientRPC;

    template <class RequestHandler, class ClientFunction>
    void register_and_perform_three_requests(RequestHandler handler, ClientFunction client_function)
    {
        int counter{};
        auto run_client_function =
            [&counter, &client_function, &server_shutdown = this->server_shutdown](const asio::yield_context& yield)
        {
            typename ClientRPC::Request request;
            typename ClientRPC::Response response;
            client_function(request, response, yield);
            ++counter;
            if (counter == 3)
            {
                server_shutdown.initiate();
            }
        };
        test::spawn_and_run(
            this->grpc_context,
            [&](const asio::yield_context& yield)
            {
                agrpc::register_yield_handler<RPC>(this->get_executor(), this->service, handler, yield);
            },
            run_client_function, run_client_function, run_client_function);
    }

    test::TestServer<SERVER_REQUEST> test_server{this->service, this->server_context};
    test::ServerShutdownInitiator server_shutdown{*this->server};
};

TEST_CASE_TEMPLATE("ServerRPC can be destructed without being started", RPC, test::UnaryServerRPC,
                   test::ServerStreamingServerRPC, test::BidirectionalStreamingServerRPC, test::GenericServerRPC)
{
    test::GrpcClientServerTest test;
    CHECK_NOTHROW([[maybe_unused]] RPC rpc{test.get_executor()});
}

TEST_CASE_FIXTURE(ServerRPCTest<test::UnaryServerRPC>, "ServerRPC unary success")
{
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    register_and_perform_three_requests(
        [&](test::UnaryServerRPC& rpc, test::msg::Request& client_request, const asio::yield_context& yield)
        {
            CHECK_EQ(42, client_request.integer());
            CHECK(rpc.send_initial_metadata(yield));
            if (use_finish_with_error)
            {
                CHECK(rpc.finish_with_error(test::create_already_exists_status(), yield));
            }
            else
            {
                test_server.response.set_integer(21);
                CHECK(rpc.finish(test_server.response, grpc::Status::OK, yield));
            }
        },
        [&](auto&, auto&, const asio::yield_context& yield)
        {
            test::client_perform_unary_success(grpc_context, *stub, yield, {use_finish_with_error});
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::ClientStreamingServerRPC>, "ServerRPC client streaming success")
{
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    register_and_perform_three_requests(
        [&](test::ClientStreamingServerRPC& rpc, const asio::yield_context& yield)
        {
            CHECK(rpc.send_initial_metadata(yield));
            CHECK(rpc.read(test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            CHECK(rpc.read(test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            CHECK_FALSE(rpc.read(test_server.request, yield));
            test_server.response.set_integer(21);
            if (use_finish_with_error)
            {
                CHECK(rpc.finish_with_error(test::create_already_exists_status(), yield));
            }
            else
            {
                CHECK(rpc.finish(test_server.response, grpc::Status::OK, yield));
            }
        },
        [&](auto&, auto& response, const asio::yield_context& yield)
        {
            grpc::ClientContext client_context;
            test::set_default_deadline(client_context);
            test::ClientAsyncWriter<false> writer;
            CHECK(agrpc::request(&test::v1::Test::Stub::PrepareAsyncClientStreaming, stub, client_context, writer,
                                 response, yield));
            test::client_perform_client_streaming_success(response, *writer, yield, {use_finish_with_error});
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::ServerStreamingServerRPC>, "ServerRPC server streaming success")
{
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    register_and_perform_three_requests(
        [&](test::ServerStreamingServerRPC& rpc, test::msg::Request& client_request, const asio::yield_context& yield)
        {
            CHECK_EQ(42, client_request.integer());
            CHECK(rpc.send_initial_metadata(yield));
            test_server.response.set_integer(21);
            CHECK(rpc.write(test_server.response, grpc::WriteOptions{}, yield));
            test_server.response.set_integer(10);
            if (use_write_and_finish)
            {
                CHECK(rpc.write_and_finish(test_server.response, grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.write(test_server.response, yield));
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            rpc.done(yield);
            CHECK_FALSE(rpc.context().IsCancelled());
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            ClientRPC rpc{grpc_context, test::set_default_deadline};
            request.set_integer(42);
            rpc.start(*stub, request, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(21, response.integer());
            CHECK(rpc.read(response, yield));
            CHECK_EQ(10, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(10, response.integer());
            CHECK(rpc.finish(yield).ok());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::ServerStreamingServerRPC>,
                  "ServerRPC server streaming no finish causes cancellation")
{
    register_and_perform_three_requests(
        [&](test::ServerStreamingServerRPC& rpc, auto&, const asio::yield_context& yield)
        {
            CHECK(rpc.write(test_server.response, yield));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            ClientRPC rpc{grpc_context, test::set_default_deadline};
            rpc.start(*stub, request, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::BidirectionalStreamingServerRPC>, "ServerRPC bidi streaming success")
{
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    register_and_perform_three_requests(
        [&](test::BidirectionalStreamingServerRPC& rpc, const asio::yield_context& yield)
        {
            CHECK(rpc.send_initial_metadata(yield));
            CHECK(rpc.read(test_server.request, yield));
            CHECK_FALSE(rpc.read(test_server.request, yield));
            test_server.response.set_integer(21);
            CHECK(rpc.write(test_server.response, grpc::WriteOptions{}, yield));
            test_server.response.set_integer(10);
            if (use_write_and_finish)
            {
                CHECK(rpc.write_and_finish(test_server.response, grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.write(test_server.response, yield));
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            rpc.done(yield);
            CHECK_FALSE(rpc.context().IsCancelled());
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            ClientRPC rpc{grpc_context, test::set_default_deadline};
            rpc.start(*stub, yield);
            request.set_integer(42);
            CHECK(rpc.write(request, yield));
            CHECK(rpc.writes_done(yield));
            CHECK(rpc.read(response, yield));
            CHECK_EQ(21, response.integer());
            CHECK(rpc.read(response, yield));
            CHECK_EQ(10, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(10, response.integer());
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::GenericServerRPC>, "GenericStreamingClientRPC success")
{
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    register_and_perform_three_requests(
        [&](test::GenericServerRPC& rpc, const asio::yield_context& yield)
        {
            CHECK(rpc.send_initial_metadata(yield));
            grpc::ByteBuffer request;
            CHECK(rpc.read(request, yield));
            CHECK_FALSE(rpc.read(request, yield));
            CHECK_EQ(42, test::grpc_buffer_to_message<test::msg::Request>(request).integer());
            test::msg::Response response;
            response.set_integer(21);
            if (use_write_and_finish)
            {
                CHECK(rpc.write_and_finish(test::message_to_grpc_buffer(response), grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.write(test::message_to_grpc_buffer(response), yield));
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            rpc.done(yield);
            CHECK_FALSE(rpc.context().IsCancelled());
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            test::v1::Test::Stub stub{channel};
            test::BidirectionalStreamingClientRPC rpc{grpc_context, test::set_default_deadline};
            rpc.start(stub, yield);
            request.set_integer(42);
            CHECK(rpc.write(request, yield));
            CHECK(rpc.writes_done(yield));
            CHECK(rpc.read(response, yield));
            CHECK_EQ(21, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(21, response.integer());
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}