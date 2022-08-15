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
#include "utils/high_level_client.hpp"
#include "utils/io_context_test.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"
#include "utils/time.hpp"

#include <agrpc/high_level_client.hpp>
#include <agrpc/wait.hpp>

#include <future>

TEST_CASE_TEMPLATE("RPC::request automatically finishes RPC on error", RPC, test::UnaryRPC, test::ClientStreamingRPC,
                   test::ServerStreamingRPC, test::BidirectionalStreamingRPC)
{
    test::HighLevelClientTest<RPC> test;
    test.server->Shutdown();
    test.client_context.set_deadline(test::ten_milliseconds_from_now());
    test.request_rpc(
        [](RPC&& rpc)
        {
            CHECK_FALSE(rpc.ok());
            CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == rpc.status_code() ||
                           grpc::StatusCode::UNAVAILABLE == rpc.status_code()),
                          rpc.status_code());
        });
    test.grpc_context.run();
}

TEST_CASE_TEMPLATE("RPC::read_initial_metadata automatically finishes RPC on error", RPC, test::ClientStreamingRPC,
                   test::ServerStreamingRPC)
{
    test::HighLevelClientTest<RPC> test;
    test.spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test.test_server.request_rpc(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test.request_rpc(yield);
            CHECK(rpc.ok());
            test.client_context.TryCancel();
            CHECK_FALSE(rpc.read_initial_metadata(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
            test.server->Shutdown();
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::UnaryRPC>,
                  "RPC::request can have UseSender as default completion token")
{
    using RPC = agrpc::UseSender::as_default_on_t<agrpc::BasicRPC<&test::v1::Test::Stub::PrepareAsyncUnary>>;
    bool ok{};
    test::DeleteGuard guard{};
    bool use_submit{};
    SUBCASE("submit") { use_submit = true; }
    SUBCASE("start") {}
    spawn_and_run(
        [&](const asio::yield_context& yield)
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

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ServerStreamingRPC>, "ServerStreamingRPC::read successfully")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK_EQ(42, test_server.request.integer());
            test_server.response.set_integer(1);
            CHECK(agrpc::write_and_finish(test_server.responder, test_server.response, {},
                                          grpc::Status{grpc::StatusCode::ALREADY_EXISTS, ""}, yield));
        },
        [&](const asio::yield_context& yield)
        {
            request.set_integer(42);
            auto rpc = test::ServerStreamingRPC::request(grpc_context, *stub, client_context, request, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(1, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ServerStreamingRPC>,
                  "ServerStreamingRPC::read automatically finishes on error")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test_server.request_rpc(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::ServerStreamingRPC::request(grpc_context, *stub, client_context, request, yield);
            client_context.TryCancel();
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
            server->Shutdown();
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ClientStreamingRPC>, "ClientStreamingRPC::write successfully")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            test_server.response.set_integer(1);
            CHECK_FALSE(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::CANCELLED, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::ClientStreamingRPC::request(grpc_context, *stub, client_context, response, yield);
            request.set_integer(42);
            grpc::WriteOptions options{};
            CHECK(rpc.write(request, options.set_last_message(), yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ClientStreamingRPC>,
                  "ClientStreamingRPC::write automatically finishes on error")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test_server.request_rpc(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::ClientStreamingRPC::request(grpc_context, *stub, client_context, response, yield);
            client_context.TryCancel();
            CHECK_FALSE(rpc.write(request, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
            server->Shutdown();
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ClientStreamingRPC>,
                  "ClientStreamingRPC::finish can be called multiple times on a successful RPC")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::ClientStreamingRPC::request(grpc_context, *stub, client_context, response, yield);
            CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
            CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ClientStreamingRPC>,
                  "ClientStreamingRPC::finish can be called after set_last_message")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::ClientStreamingRPC::request(grpc_context, *stub, client_context, response, yield);
            CHECK(rpc.write(request, grpc::WriteOptions{}.set_last_message(), yield));
            CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
            CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ClientStreamingRPC>,
                  "ClientStreamingRPC::finish can be called multiple times on a failed RPC")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test_server.request_rpc(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::ClientStreamingRPC::request(grpc_context, *stub, client_context, response, yield);
            client_context.TryCancel();
            CHECK_FALSE(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
            CHECK_FALSE(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
            server->Shutdown();
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::ClientStreamingRPC>,
                  "ClientStreamingRPC::finish can be called multiple times using sender")
{
    bool expected_ok = true;
    auto expected_status_code = grpc::StatusCode::OK;
    SUBCASE("success") {}
    SUBCASE("failure")
    {
        expected_ok = false;
        expected_status_code = grpc::StatusCode::CANCELLED;
    }
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test_server.request_rpc(yield);
            if (expected_ok)
            {
                CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
            }
        },
        [&](asio::yield_context yield)
        {
            auto rpc = std::make_unique<test::ClientStreamingRPC>(
                test::ClientStreamingRPC::request(grpc_context, *stub, client_context, response, yield));
            if (!expected_ok)
            {
                client_context.TryCancel();
            }
            auto& rpc_ref = *rpc;
            asio::execution::submit(
                rpc_ref.finish(agrpc::use_sender),
                test::FunctionAsReceiver{[&, rpc = std::move(rpc)](bool ok) mutable
                                         {
                                             CHECK_EQ(expected_ok, ok);
                                             CHECK_EQ(expected_status_code, rpc->status_code());
                                             asio::execution::submit(
                                                 rpc_ref.finish(agrpc::use_sender),
                                                 test::FunctionAsReceiver{[&, rpc = std::move(rpc)](bool ok)
                                                                          {
                                                                              CHECK_EQ(expected_ok, ok);
                                                                              CHECK_EQ(expected_status_code,
                                                                                       rpc->status_code());
                                                                              server->Shutdown();
                                                                          }});
                                         }});
        });
}

TEST_CASE_FIXTURE(test::HighLevelClientTest<test::UnaryRPC>, "RPC::request generic unary RPC successfully")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK_EQ(42, test_server.request.integer());
            test_server.response.set_integer(24);
            CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            using RPC = agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC>;
            grpc::GenericStub generic_stub{channel};
            request.set_integer(42);
            auto request_buf = test::message_to_grpc_buffer(request);
            grpc::ByteBuffer response_buf;
            auto rpc = RPC::request(grpc_context, "/test.v1.Test/Unary", generic_stub, client_context, request_buf,
                                    response_buf, yield);
            CHECK(rpc.ok());
            response = test::grpc_buffer_to_message<decltype(response)>(response_buf);
            CHECK_EQ(24, response.integer());
        });
}

TEST_CASE_FIXTURE(test::GrpcClientServerTestBase,
                  "RPC::request generic unary RPC automatically retrieves grpc::Status on error")
{
    test::spawn_and_run(grpc_context,
                        [&](const asio::yield_context& yield)
                        {
                            using RPC = agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC>;
                            grpc::GenericStub generic_stub{channel};
                            grpc::ByteBuffer request_buf;
                            grpc::ByteBuffer response_buf;
                            client_context.set_deadline(test::now());
                            auto rpc = RPC::request(grpc_context, "/test.v1.Test/Unary", generic_stub, client_context,
                                                    request_buf, response_buf, yield);
                            CHECK_FALSE(rpc.ok());
                            CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == rpc.status_code() ||
                                           grpc::StatusCode::UNAVAILABLE == rpc.status_code()),
                                          rpc.status_code());
                        });
}

struct HighLevelClientBidiTest : test::HighLevelClientTest<test::BidirectionalStreamingRPC>, test::IoContextTest
{
    template <class ServerFunc, class ClientFunc>
    void run_server_client_on_separate_threads(ServerFunc server_func, ClientFunc client_func)
    {
        asio::spawn(io_context,
                    [client_func, g = get_work_tracking_executor()](const asio::yield_context& yield)
                    {
                        client_func(yield);
                    });
        run_io_context_detached();
        spawn_and_run(
            [server_func](const asio::yield_context& yield)
            {
                server_func(yield);
            });
    }
};

TEST_CASE_FIXTURE(HighLevelClientBidiTest, "BidirectionalStreamingRPC success")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            test_server.response.set_integer(1);
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            CHECK(agrpc::write(test_server.responder, test_server.response, yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::BidirectionalStreamingRPC::request(grpc_context, *stub, client_context, yield);
            request.set_integer(42);
            CHECK(rpc.write(request, yield));
            CHECK(rpc.read(response, yield));
            CHECK_EQ(1, response.integer());
            CHECK_FALSE(rpc.write(request, yield));
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(1, response.integer());
            CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
            CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(HighLevelClientBidiTest, "BidirectionalStreamingRPC automatically finishes when read returns false")
{
    bool concurrent_write{};
    SUBCASE("no concurrent write") {}
    SUBCASE("concurrent write") { concurrent_write = true; }
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            agrpc::write_last(test_server.responder, test_server.response, {},
                              asio::bind_executor(grpc_context, [](bool) {}));
            CHECK(agrpc::finish(test_server.responder, grpc::Status{grpc::StatusCode::ALREADY_EXISTS, ""}, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::BidirectionalStreamingRPC::request(grpc_context, *stub, client_context, yield);
            CHECK(rpc.read(response, yield));
            std::promise<void> promise;
            if (concurrent_write)
            {
                rpc.write(request,
                          [&](bool ok)
                          {
                              CHECK_FALSE(ok);
                              promise.set_value();
                          });
            }
            else
            {
                promise.set_value();
            }
            CHECK_FALSE(rpc.read(response, yield));
            promise.get_future().get();
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.status_code());
            CHECK_FALSE(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(HighLevelClientBidiTest,
                  "BidirectionalStreamingRPC automatically finishes when TryCancel before write+read")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::BidirectionalStreamingRPC::request(grpc_context, *stub, client_context, yield);
            client_context.TryCancel();
            std::promise<void> promise;
            rpc.read(response,
                     [&](bool ok)
                     {
                         CHECK_FALSE(ok);
                         promise.set_value();
                     });
            CHECK_FALSE(rpc.write(request, yield));
            promise.get_future().get();
            CHECK_FALSE(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
        });
}

TEST_CASE_FIXTURE(HighLevelClientBidiTest, "BidirectionalStreamingRPC can finish on failed write while reading")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            // test_server.response.set_integer(1);
            // CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            // CHECK_EQ(42, test_server.request.integer());
            // CHECK(agrpc::write(test_server.responder, test_server.response, yield));
            // agrpc::write_last(test_server.responder, test_server.response, {},
            //                   asio::bind_executor(grpc_context, [](bool) {}));
            // CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test::BidirectionalStreamingRPC::request(grpc_context, *stub, client_context, yield);
            client_context.TryCancel();
            std::promise<void> promise;
            rpc.read(response,
                     [&](bool ok)
                     {
                         CHECK_FALSE(ok);
                         promise.set_value();
                     });
            CHECK_FALSE(rpc.write(request, yield));
            promise.get_future().get();
            CHECK_FALSE(rpc.finish(yield));
            // CHECK(rpc.write(request, yield));
            // CHECK(rpc.read(response, yield));
            // CHECK_EQ(1, response.integer());
            // CHECK(rpc.finish(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
            // CHECK(rpc.finish(yield));
            // CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
        });
}
