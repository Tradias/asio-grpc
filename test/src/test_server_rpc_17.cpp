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
#include "utils/future.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_generic_client_server_test.hpp"
#include "utils/introspect_rpc.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"
#include "utils/server_rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <agrpc/register_yield_handler.hpp>
#include <agrpc/server_rpc.hpp>

#include <cstddef>

template <class RPC>
struct ServerRPCTest : std::conditional_t<(agrpc::ServerRPCType::GENERIC == RPC::TYPE),
                                          test::GrpcGenericClientServerTest, test::GrpcClientServerTest>
{
    static constexpr auto SERVER_REQUEST = test::IntrospectRPC<RPC>::SERVER_REQUEST;

    using ServerRPC = RPC;
    using ClientRPC = typename test::IntrospectRPC<RPC>::ClientRPC;

    ServerRPCTest() = default;

    explicit ServerRPCTest(bool)
    {
        if constexpr (RPC::Traits::NOTIFY_WHEN_DONE)
        {
            SUBCASE("implicit notify when done") {}
            SUBCASE("explicit notify when done") { use_notify_when_done = true; }
        }
    }

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

    auto set_up_notify_when_done([[maybe_unused]] RPC& rpc)
    {
        if constexpr (RPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (use_notify_when_done)
            {
                return rpc.done(asio::use_future);
            }
        }
        return std::future<void>{};
    }

    void check_notify_when_done([[maybe_unused]] std::future<void>& future, [[maybe_unused]] RPC& rpc,
                                [[maybe_unused]] const asio::yield_context& yield)
    {
        if constexpr (RPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (use_notify_when_done)
            {
                CHECK(test::wait_for_future(this->grpc_context, future, yield));
                CHECK_FALSE(rpc.context().IsCancelled());
            }
        }
    }

    test::ServerShutdownInitiator server_shutdown{*this->server};
    bool use_notify_when_done{};
};

TEST_CASE_TEMPLATE("ServerRPC can be destructed without being started", RPC, test::UnaryServerRPC,
                   test::ServerStreamingServerRPC, test::BidirectionalStreamingServerRPC, test::GenericServerRPC)
{
    test::GrpcClientServerTest test;
    CHECK_NOTHROW([[maybe_unused]] RPC rpc{test.get_executor()});
}

TEST_CASE_TEMPLATE("ServerRPC unary success", RPC, test::UnaryServerRPC, test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, test::msg::Request& request, const asio::yield_context& yield)
        {
            CHECK_EQ(42, request.integer());
            CHECK(rpc.send_initial_metadata(yield));
            if (use_finish_with_error)
            {
                CHECK(rpc.finish_with_error(test::create_already_exists_status(), yield));
            }
            else
            {
                typename RPC::Response response;
                response.set_integer(21);
                CHECK(rpc.finish(response, grpc::Status::OK, yield));
            }
        },
        [&](auto&, auto&, const asio::yield_context& yield)
        {
            test::client_perform_unary_success(test.grpc_context, *test.stub, yield, {use_finish_with_error});
        });
}

TEST_CASE_TEMPLATE("ServerRPC client streaming success", RPC, test::ClientStreamingServerRPC,
                   test::NotifyWhenDoneClientStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, const asio::yield_context& yield)
        {
            auto future = test.set_up_notify_when_done(rpc);
            CHECK(rpc.send_initial_metadata(yield));
            typename RPC::Request request;
            CHECK(rpc.read(request, yield));
            CHECK_EQ(42, request.integer());
            CHECK(rpc.read(request, yield));
            CHECK_EQ(42, request.integer());
            CHECK_FALSE(rpc.read(request, yield));
            typename RPC::Response response;
            response.set_integer(21);
            if (use_finish_with_error)
            {
                CHECK(rpc.finish_with_error(test::create_already_exists_status(), yield));
            }
            else
            {
                CHECK(rpc.finish(response, grpc::Status::OK, yield));
            }
            test.check_notify_when_done(future, rpc, yield);
        },
        [&](auto&, auto& response, const asio::yield_context& yield)
        {
            grpc::ClientContext client_context;
            test::set_default_deadline(client_context);
            test::ClientAsyncWriter<false> writer;
            CHECK(agrpc::request(&test::v1::Test::Stub::PrepareAsyncClientStreaming, test.stub, client_context, writer,
                                 response, yield));
            test::client_perform_client_streaming_success(response, *writer, yield, {use_finish_with_error});
        });
}

TEST_CASE_TEMPLATE("ServerRPC server streaming success", RPC, test::ServerStreamingServerRPC,
                   test::NotifyWhenDoneServerStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, test::msg::Request& request, const asio::yield_context& yield)
        {
            auto future = test.set_up_notify_when_done(rpc);
            CHECK_EQ(42, request.integer());
            CHECK(rpc.send_initial_metadata(yield));
            typename RPC::Response response;
            response.set_integer(21);
            CHECK(rpc.write(response, grpc::WriteOptions{}, yield));
            response.set_integer(10);
            if (use_write_and_finish)
            {
                CHECK(rpc.write_and_finish(response, grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.write(response, yield));
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            test.check_notify_when_done(future, rpc, yield);
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            typename ServerRPCTest<RPC>::ClientRPC rpc{test.grpc_context, test::set_default_deadline};
            request.set_integer(42);
            rpc.start(*test.stub, request, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(21, response.integer());
            CHECK(rpc.read(response, yield));
            CHECK_EQ(10, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(10, response.integer());
            CHECK(rpc.finish(yield).ok());
        });
}

TEST_CASE_TEMPLATE("ServerRPC server streaming no finish causes cancellation", RPC, test::ServerStreamingServerRPC,
                   test::NotifyWhenDoneServerStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    test.register_and_perform_three_requests(
        [&](RPC& rpc, auto&, const asio::yield_context& yield)
        {
            auto future = test.set_up_notify_when_done(rpc);
            typename RPC::Response response;
            CHECK(rpc.write(response, yield));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            typename ServerRPCTest<RPC>::ClientRPC rpc{test.grpc_context, test::set_default_deadline};
            rpc.start(*test.stub, request, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_TEMPLATE("ServerRPC bidi streaming success", RPC, test::BidirectionalStreamingServerRPC,
                   test::NotifyWhenDoneBidirectionalStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, const asio::yield_context& yield)
        {
            auto future = test.set_up_notify_when_done(rpc);
            CHECK(rpc.send_initial_metadata(yield));
            typename RPC::Request request;
            CHECK(rpc.read(request, yield));
            CHECK_FALSE(rpc.read(request, yield));
            typename RPC::Response response;
            response.set_integer(21);
            CHECK(rpc.write(response, grpc::WriteOptions{}, yield));
            response.set_integer(10);
            if (use_write_and_finish)
            {
                CHECK(rpc.write_and_finish(response, grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.write(response, yield));
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            test.check_notify_when_done(future, rpc, yield);
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            typename ServerRPCTest<RPC>::ClientRPC rpc{test.grpc_context, test::set_default_deadline};
            rpc.start(*test.stub, yield);
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

TEST_CASE_TEMPLATE("GenericStreamingClientRPC success", RPC, test::GenericServerRPC,
                   test::NotifyWhenDoneGenericServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, const asio::yield_context& yield)
        {
            auto future = test.set_up_notify_when_done(rpc);
            CHECK(rpc.send_initial_metadata(yield));
            typename RPC::Request request;
            CHECK(rpc.read(request, yield));
            CHECK_FALSE(rpc.read(request, yield));
            CHECK_EQ(42, test::grpc_buffer_to_message<test::msg::Request>(request).integer());
            test::msg::Response response;
            response.set_integer(21);
            if (use_write_and_finish)
            {
                typename RPC::Response raw_response = test::message_to_grpc_buffer(response);
                CHECK(rpc.write_and_finish(raw_response, grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.write(test::message_to_grpc_buffer(response), yield));
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            test.check_notify_when_done(future, rpc, yield);
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            test::v1::Test::Stub stub{test.channel};
            test::BidirectionalStreamingClientRPC rpc{test.grpc_context, test::set_default_deadline};
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