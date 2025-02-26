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
#include "utils/client_rpc.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/doctest.hpp"
#include "utils/future.hpp"
#include "utils/introspect_rpc.hpp"
#include "utils/protobuf.hpp"
#include "utils/requestMessageFactory.hpp"
#include "utils/rpc.hpp"
#include "utils/server_rpc.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <agrpc/read.hpp>
#include <agrpc/server_rpc.hpp>
#include <agrpc/waiter.hpp>

template <class ServerRPC>
struct ServerRPCTest : test::ClientServerRPCTest<typename test::IntrospectRPC<ServerRPC>::ClientRPC, ServerRPC>
{
    ServerRPCTest() = default;

    explicit ServerRPCTest(bool)
    {
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            SUBCASE("implicit notify when done") {}
            SUBCASE("explicit notify when done") { use_notify_when_done_ = true; }
        }
    }

    auto set_up_notify_when_done([[maybe_unused]] ServerRPC& rpc)
    {
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (use_notify_when_done_)
            {
                return rpc.wait_for_done(asio::use_future);
            }
        }
        return std::future<void>{};
    }

    void check_notify_when_done([[maybe_unused]] std::future<void>& future, [[maybe_unused]] ServerRPC& rpc,
                                [[maybe_unused]] const asio::yield_context& yield)
    {
        if constexpr (ServerRPC::Traits::NOTIFY_WHEN_DONE)
        {
            if (use_notify_when_done_)
            {
                CHECK(test::wait_for_future(this->grpc_context, future, yield));
                CHECK_FALSE(rpc.context().IsCancelled());
            }
        }
    }

    bool use_notify_when_done_{};
};

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

TEST_CASE_TEMPLATE("ServerRPC unary start+finish success", RPC, test::UnaryServerRPC,
                   test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, test::msg::Request& request, const asio::yield_context& yield)
        {
            CHECK_EQ(42, request.integer());
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
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            request.set_integer(42);
            typename ServerRPCTest<RPC>::ClientRPC rpc{test.grpc_context, test::set_default_deadline};
            rpc.start(*test.stub, request);
            request = {};
            const auto status = rpc.finish(response, yield);
            if (use_finish_with_error)
            {
                CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, status.error_code());
            }
            else
            {
                CHECK(status.ok());
                CHECK_EQ(21, response.integer());
            }
        });
}

TEST_CASE_TEMPLATE("Unary ClientRPC/ServerRPC read/send_initial_metadata successfully", RPC, test::UnaryServerRPC,
                   test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_start{};
    SUBCASE("use request") {}
    SUBCASE("use start") { use_start = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, auto&, const asio::yield_context& yield)
        {
            auto future = test.set_up_notify_when_done(rpc);
            rpc.context().AddInitialMetadata("test", "a");
            CHECK(rpc.send_initial_metadata(yield));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            if (use_start)
            {
                typename ServerRPCTest<RPC>::ClientRPC rpc{test.grpc_context, test::set_default_deadline};
                rpc.start(*test.stub, request);
                rpc.read_initial_metadata(yield);
                CHECK_EQ(0, rpc.context().GetServerInitialMetadata().find("test")->second.compare("a"));
            }
            else
            {
                const auto client_context = test::create_client_context();
                CHECK_EQ(grpc::StatusCode::CANCELLED,
                         test.request_rpc(*client_context, request, response, yield).error_code());
                CHECK_EQ(0, client_context->GetServerInitialMetadata().find("test")->second.compare("a"));
            }
        });
}

struct GetYield
{
    static auto& get(test::msg::Request&, const asio::yield_context& yield) { return yield; }
    static auto& get(const asio::yield_context& yield) { return yield; }
};

TEST_CASE_TEMPLATE("Streaming ClientRPC/ServerRPC read/send_initial_metadata successfully", RPC,
                   test::ClientStreamingServerRPC, test::NotifyWhenDoneClientStreamingServerRPC,
                   test::ServerStreamingServerRPC, test::NotifyWhenDoneServerStreamingServerRPC,
                   test::BidirectionalStreamingServerRPC, test::NotifyWhenDoneBidirectionalStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    test.register_and_perform_three_requests(
        [&](RPC& rpc, auto&&... args)
        {
            auto future = test.set_up_notify_when_done(rpc);
            rpc.context().AddInitialMetadata("test", "a");
            CHECK(rpc.send_initial_metadata(GetYield::get(args...)));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            CHECK(test.start_rpc(rpc, request, response, yield));
            CHECK(rpc.read_initial_metadata(yield));
            CHECK_EQ(0, rpc.context().GetServerInitialMetadata().find("test")->second.compare("a"));
        });
}

TEST_CASE_TEMPLATE("ServerRPC/ClientRPC client streaming success", RPC, test::ClientStreamingServerRPC,
                   test::NotifyWhenDoneClientStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    bool set_last_message{};
    SUBCASE("no last_message") {}
    SUBCASE("last_message") { set_last_message = true; }
    test.register_and_perform_three_requests(
        test::RPCHandlerWithRequestMessageFactory{
            [&](RPC& rpc, const asio::yield_context& yield)
            {
                auto future = test.set_up_notify_when_done(rpc);
                typename RPC::Request request;
                CHECK(rpc.read(request, yield));
                CHECK_EQ(1, request.integer());
                CHECK(rpc.read(request, yield));
                CHECK_EQ(2, request.integer());
                CHECK_FALSE(rpc.read(request, yield));
                typename RPC::Response response;
                response.set_integer(11);
                if (use_finish_with_error)
                {
                    CHECK(rpc.finish_with_error(test::create_already_exists_status(), yield));
                }
                else
                {
                    CHECK(rpc.finish(response, grpc::Status::OK, yield));
                }
                test.check_notify_when_done(future, rpc, yield);
            }  //
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            test.start_rpc(rpc, request, response, yield);
            request.set_integer(1);
            CHECK(rpc.write(request, yield));
            request.set_integer(2);
            if (set_last_message)
            {
                CHECK(rpc.write(request, grpc::WriteOptions{}.set_last_message(), yield));
            }
            else
            {
                CHECK(rpc.write(request, yield));
            }
            if (use_finish_with_error)
            {
                CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.finish(yield).error_code());
            }
            else
            {
                CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
                CHECK_EQ(11, response.integer());
            }
        });
}

TEST_CASE_TEMPLATE("ServerRPC/ClientRPC server streaming success", RPC, test::ServerStreamingServerRPC,
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
            CHECK_EQ(1, request.integer());
            typename RPC::Response response;
            response.set_integer(11);
            CHECK(rpc.write(response, grpc::WriteOptions{}, yield));
            response.set_integer(12);
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
            auto rpc = test.create_rpc();
            request.set_integer(1);
            test.start_rpc(rpc, request, response, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(11, response.integer());
            CHECK(rpc.read(response, yield));
            CHECK_EQ(12, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_TEMPLATE("ServerRPC/ClientRPC server streaming no finish causes cancellation", RPC,
                   test::ServerStreamingServerRPC, test::NotifyWhenDoneServerStreamingServerRPC)
{
    ServerRPCTest<RPC> test{};
    test.register_and_perform_three_requests(
        [&](RPC& rpc, auto&, const asio::yield_context& yield)
        {
            typename RPC::Response response;
            CHECK(rpc.write(response, yield));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            test.start_rpc(rpc, request, response, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_TEMPLATE("ServerRPC/ClientRPC bidi streaming success", RPC, test::BidirectionalStreamingServerRPC,
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
            typename RPC::Request request;
            CHECK(rpc.read(request, yield));
            CHECK_EQ(1, request.integer());
            CHECK_FALSE(rpc.read(request, yield));
            typename RPC::Response response;
            response.set_integer(11);
            CHECK(rpc.write(response, grpc::WriteOptions{}, yield));
            response.set_integer(12);
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
            auto rpc = test.create_rpc();
            test.start_rpc(rpc, request, response, yield);
            request.set_integer(1);
            CHECK(rpc.write(request, yield));
            CHECK(rpc.writes_done(yield));
            CHECK(rpc.read(response, yield));
            CHECK_EQ(11, response.integer());
            CHECK(rpc.read(response, yield));
            CHECK_EQ(12, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(12, response.integer());
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::BidirectionalStreamingServerRPC>,
                  "BidirectionalStreamingServerRPC concurrent read+finish")
{
    bool order{};
    register_and_perform_requests(
        [&](ServerRPC& rpc, const asio::yield_context& yield)
        {
            Request request;
            CHECK(rpc.read(request, yield));
            std::promise<bool> promise;
            rpc.read(request,
                     [&](bool ok)
                     {
                         promise.set_value(ok);
                     });
            CHECK(rpc.finish(grpc::Status{grpc::StatusCode::ALREADY_EXISTS, ""}, yield));
            CHECK_FALSE(order);
            CHECK_FALSE(promise.get_future().get());
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, request, response, yield);
            CHECK(rpc.write(request, yield));
            wait(test::one_second_from_now(), yield);
            order = true;
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::GenericServerRPC>, "ServerRPC/ClientRPC generic unary RPC success")
{
    int option{};
    SUBCASE("executor overload") {}
    SUBCASE("GrpcContext overload") { option = 1; }
    SUBCASE("start+finish") { option = 2; }
    register_and_perform_three_requests(
        [&](test::GenericServerRPC& rpc, const asio::yield_context& yield)
        {
            grpc::ByteBuffer request;
            CHECK(rpc.read(request, yield));
            CHECK_EQ(1, test::grpc_buffer_to_message<test::msg::Request>(request).integer());
            test::msg::Response response;
            response.set_integer(11);
            CHECK(rpc.write_and_finish(test::message_to_grpc_buffer(response), grpc::Status::OK, yield));
        },
        [&](grpc::ByteBuffer& request, grpc::ByteBuffer& response, const asio::yield_context& yield)
        {
            grpc::ClientContext client_context;
            test::set_default_deadline(client_context);
            test::msg::Request typed_request;
            typed_request.set_integer(1);
            request = test::message_to_grpc_buffer(typed_request);
            auto status = [&]
            {
                if (0 == option)
                {
                    return test::GenericUnaryClientRPC::request(get_executor(), "/test.v1.Test/Unary", *stub,
                                                                client_context, request, response, yield);
                }
                else if (1 == option)
                {
                    return test::GenericUnaryClientRPC::request(grpc_context, "/test.v1.Test/Unary", *stub,
                                                                client_context, request, response, yield);
                }
                test::GenericUnaryClientRPC rpc{grpc_context, test::set_default_deadline};
                rpc.start("/test.v1.Test/Unary", *stub, request);
                return rpc.finish(response, yield);
            }();
            CHECK_EQ(grpc::StatusCode::OK, status.error_code());
            CHECK_EQ(11, test::grpc_buffer_to_message<test::msg::Response>(response).integer());
        });
}

TEST_CASE_TEMPLATE("ServerRPC/ClientRPC generic streaming success", RPC, test::GenericServerRPC,
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
            auto rpc = test.create_rpc();
            CHECK(test.start_rpc(rpc, request, response, yield));

            test::msg::Request typed_request;
            typed_request.set_integer(42);
            CHECK(rpc.write(test::message_to_grpc_buffer(typed_request), yield));
            CHECK(rpc.writes_done(yield));

            CHECK(rpc.read(response, yield));
            CHECK_EQ(21, test::grpc_buffer_to_message<test::msg::Response>(response).integer());

            response.Clear();
            CHECK_FALSE(rpc.read(response, yield));

            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE("ServerRPC/ClientRPC bidi streaming on io_context success")
{
    using RPC = test::NotifyWhenDoneBidirectionalStreamingServerRPC;
    ServerRPCTest<RPC> test{true};
    asio::io_context io_context{1};
    const auto io_context_thread_id = std::this_thread::get_id();
    std::thread::id final_thread_id{};
    agrpc::register_yield_rpc_handler<RPC>(
        test.get_executor(), test.service,
        [&](RPC& rpc, const asio::yield_context& yield)
        {
            CHECK_EQ(io_context_thread_id, std::this_thread::get_id());
            auto future = test.set_up_notify_when_done(rpc);
            RPC::Request request;
            CHECK(rpc.read(request, yield));
            CHECK_EQ(1, request.integer());
            RPC::Response response;
            response.set_integer(11);
            CHECK(rpc.write_and_finish(response, grpc::Status::OK, yield));
            CHECK_EQ(io_context_thread_id, std::this_thread::get_id());
            test.check_notify_when_done(future, rpc, yield);
        },
        asio::bind_executor(io_context,
                            [&](auto&& ep)
                            {
                                final_thread_id = std::this_thread::get_id();
                                test::RethrowFirstArg{}(ep);
                            }));
    auto client_function = [&](RPC::Request& request, RPC::Response& response, const asio::yield_context& yield)
    {
        auto rpc = test.create_rpc();
        test.start_rpc(rpc, request, response, yield);
        request.set_integer(1);
        CHECK(rpc.write(request, yield));
        CHECK(rpc.writes_done(yield));
        CHECK(rpc.read(response, yield));
        CHECK_EQ(11, response.integer());
        CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
    };
    test.spawn_client_functions(io_context, client_function, client_function, client_function);
    std::thread t{[&]
                  {
                      test.grpc_context.run_completion_queue();
                  }};
    io_context.run();
    t.join();
    CHECK_EQ(final_thread_id, std::this_thread::get_id());
}

TEST_CASE_FIXTURE(ServerRPCTest<test::UnaryServerRPC>, "Unary ServerRPC with protobuf Arena")
{
    register_and_perform_three_requests(
        test::RPCHandlerWithRequestMessageFactory{
            [&](ServerRPC& rpc, Request& request, const asio::yield_context& yield,
                test::ArenaRequestMessageFactory& factory)
            {
                CHECK_EQ(42, request.integer());
                CHECK(test::has_arena(request, factory.arena));
                rpc.finish({}, grpc::Status::OK, yield);
                CHECK_FALSE(factory.is_destroy_invoked);
            }},
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            const auto client_context = test::create_client_context();
            request.set_integer(42);
            CHECK_EQ(grpc::StatusCode::OK, request_rpc(*client_context, request, response, yield).error_code());
        });
}

TEST_CASE("ServerRPC::service_name/method_name")
{
    const auto check_eq_and_null_terminated = [](std::string_view expected, std::string_view actual)
    {
        CHECK_EQ(expected, actual);
        CHECK_EQ('\0', *(actual.data() + actual.size()));
    };
    check_eq_and_null_terminated("test.v1.Test", test::UnaryServerRPC::service_name());
    check_eq_and_null_terminated("Unary", test::UnaryServerRPC::method_name());
    using UnaryRPC = agrpc::ServerRPC<&test::v1::Test::WithAsyncMethod_Unary<test::v1::Test::Service>::RequestUnary>;
    check_eq_and_null_terminated("test.v1.Test", UnaryRPC::service_name());
    check_eq_and_null_terminated("Unary", UnaryRPC::method_name());
    check_eq_and_null_terminated("test.v1.Test", test::ClientStreamingServerRPC::service_name());
    check_eq_and_null_terminated("ClientStreaming", test::ClientStreamingServerRPC::method_name());
    check_eq_and_null_terminated("test.v1.Test", test::ServerStreamingServerRPC::service_name());
    check_eq_and_null_terminated("ServerStreaming", test::ServerStreamingServerRPC::method_name());
    check_eq_and_null_terminated("test.v1.Test", test::BidirectionalStreamingServerRPC::service_name());
    check_eq_and_null_terminated("BidirectionalStreaming", test::BidirectionalStreamingServerRPC::method_name());
}

#ifdef AGRPC_TEST_ASIO_PARALLEL_GROUP
TEST_CASE_TEMPLATE("ServerRPC resumable read can be cancelled", RPC, test::ClientStreamingServerRPC,
                   test::BidirectionalStreamingServerRPC)
{
    ServerRPCTest<RPC> test{true};
    agrpc::Waiter<void()> client_waiter;
    test.register_and_perform_requests(
        [&](RPC& rpc, const asio::yield_context& yield)
        {
            typename RPC::Request request;
            agrpc::Waiter<void(bool)> waiter;

            waiter.initiate(agrpc::read, rpc, request);
            CHECK(waiter.wait(yield));
            CHECK_EQ(1, request.integer());
            CHECK(waiter.wait(yield));
            CHECK_EQ(1, request.integer());

            waiter.initiate(agrpc::read, rpc, request);
            for (int i{}; i != 2; ++i)
            {
                auto [completion_order, ec, read_ok] =
                    asio::experimental::make_parallel_group(
                        waiter.wait(test::ASIO_DEFERRED),
                        asio::post(asio::bind_executor(test.grpc_context, test::ASIO_DEFERRED)))
                        .async_wait(asio::experimental::wait_for_one(), yield);
                CHECK_EQ(asio::error::operation_aborted, ec);
                CHECK_EQ(1, request.integer());
            }
            test::complete_immediately(test.grpc_context, client_waiter);
            CHECK_FALSE(waiter.wait(yield));

            if constexpr (agrpc::ServerRPCType::BIDIRECTIONAL_STREAMING == RPC::TYPE)
            {
                CHECK(rpc.finish(grpc::Status::OK, yield));
            }
            else
            {
                CHECK(rpc.finish({}, grpc::Status::OK, yield));
            }
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            test.start_rpc(rpc, request, response, yield);
            request.set_integer(1);
            CHECK(rpc.write(request, yield));
            client_waiter.wait(yield);
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::NotifyWhenDoneClientStreamingServerRPC>, "ServerRPC cancel wait_for_done")
{
    register_and_perform_three_requests(
        [&](ServerRPC& rpc, const asio::yield_context& yield)
        {
            asio::experimental::make_parallel_group(rpc.wait_for_done(test::ASIO_DEFERRED),
                                                    asio::post(asio::bind_executor(grpc_context, test::ASIO_DEFERRED)))
                .async_wait(asio::experimental::wait_for_one(), yield);
            CHECK_FALSE(rpc.is_done());
            CHECK(rpc.send_initial_metadata(yield));
            rpc.wait_for_done(yield);
            CHECK(rpc.is_done());
            CHECK(rpc.context().IsCancelled());
            rpc.wait_for_done(yield);
        },
        [&](Request& request, Response& response, const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            CHECK(start_rpc(rpc, request, response, yield));
            CHECK(rpc.read_initial_metadata(yield));
        });
}
#endif

// Callback
TEST_CASE_TEMPLATE("ServerRPCPtr unary success", RPC, test::UnaryServerRPC, test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCTest<RPC> test{true};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    test.register_callback_and_perform_three_requests(
        [&](typename RPC::Ptr ptr, test::msg::Request& request)
        {
            CHECK_EQ(&request, &ptr.request());
            CHECK_EQ(42, request.integer());
            auto& rpc = *ptr;
            if (use_finish_with_error)
            {
                rpc.finish_with_error(test::create_already_exists_status(),
                                      [&, ptr = std::move(ptr)](bool ok)
                                      {
                                          CHECK(ok);
                                      });
            }
            else
            {
                typename RPC::Response response;
                response.set_integer(21);
                rpc.finish(response, grpc::Status::OK,
                           [&, ptr = std::move(ptr)](bool ok)
                           {
                               CHECK(ok);
                           });
            }
        },
        [&](auto&, auto&, const asio::yield_context& yield)
        {
            test::client_perform_unary_success(test.grpc_context, *test.stub, yield, {use_finish_with_error});
        });
}

TEST_CASE_TEMPLATE("ServerRPCPtr automatic cancellation on destruction", RPC, test::UnaryServerRPC,
                   test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCTest<RPC> test{true};
    test.register_callback_and_perform_three_requests(
        [&](auto&&...) {},
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            grpc::ClientContext c;
            test::set_default_deadline(c);
            auto status = test.request_rpc(c, request, response, yield);
            const auto status_code = status.error_code();
            CHECK_MESSAGE(grpc::StatusCode::CANCELLED == status_code, status_code);
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::ClientStreamingServerRPC>, "ServerRPCPtr move-assignment/swap")
{
    ServerRPC::Ptr ptr;
    register_callback_and_perform_requests(
        [&](ServerRPC::Ptr pointer)
        {
            SUBCASE("move") { ptr = std::move(pointer); }
            SUBCASE("swap")
            {
                using std::swap;
                swap(ptr, pointer);
                CHECK_FALSE(pointer);
            }
            auto& rpc = *ptr;
            rpc.finish({}, test::create_already_exists_status(),
                       [ptr = std::move(ptr)](bool ok)
                       {
                           CHECK(ok);
                       });
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            CHECK(start_rpc(rpc, request, response, yield));
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ServerRPCTest<test::UnaryServerRPC>, "Unary ServerRPCPtr with protobuf Arena")
{
    register_callback_and_perform_three_requests(
        test::RPCHandlerWithRequestMessageFactory{
            [&](ServerRPC::Ptr ptr, Request& request, test::ArenaRequestMessageFactory& factory)
            {
                CHECK_EQ(42, request.integer());
                CHECK_EQ(&ptr.request(), &request);
                CHECK(test::has_arena(request, factory.arena));
                auto& rpc = *ptr;
                rpc.finish({}, grpc::Status::OK,
                           [ptr = std::move(ptr)](bool ok)
                           {
                               CHECK(ok);
                           });
                CHECK_FALSE(factory.is_destroy_invoked);
            }},
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            const auto client_context = test::create_client_context();
            request.set_integer(42);
            CHECK_EQ(grpc::StatusCode::OK, request_rpc(*client_context, request, response, yield).error_code());
        });
}
