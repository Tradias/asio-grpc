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

#include "utils/client_rpc.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/doctest.hpp"
#include "utils/exception.hpp"
#include "utils/introspect_rpc.hpp"
#include "utils/io_context_test.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"
#include "utils/server_rpc.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/bind_allocator.hpp>
#include <agrpc/client_rpc.hpp>
#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <agrpc/server_rpc.hpp>
#include <agrpc/waiter.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class ServerRPC>
struct ServerRPCAwaitableTest : test::ClientServerRPCTest<typename test::IntrospectRPC<ServerRPC>::ClientRPC, ServerRPC>
{
    using Base = test::ClientServerRPCTest<typename test::IntrospectRPC<ServerRPC>::ClientRPC, ServerRPC>;
    using typename Base::ClientRPC;

    template <class... ClientFunctions>
    void perform_requests_in_order(ClientFunctions&&... client_functions)
    {
        test::spawn_and_run(this->grpc_context,
                            [&](const asio::yield_context& yield)
                            {
                                (
                                    [&]
                                    {
                                        typename ClientRPC::Request request;
                                        typename ClientRPC::Response response;
                                        client_functions(request, response, yield);
                                    }(),
                                    ...);
                                this->server_shutdown.initiate();
                            });
    }

    template <class... ClientFunctions>
    void perform_requests(ClientFunctions&&... client_functions)
    {
        int counter{};
        test::spawn_and_run(
            this->grpc_context,
            [&counter, &client_functions, &server_shutdown = this->server_shutdown](const asio::yield_context& yield)
            {
                typename ClientRPC::Request request;
                typename ClientRPC::Response response;
                client_functions(request, response, yield);
                ++counter;
                if (counter == sizeof...(client_functions))
                {
                    server_shutdown.initiate();
                }
            }...);
    }

    template <class RPCHandler, class... ClientFunctions>
    void register_and_perform_requests(RPCHandler&& handler, ClientFunctions&&... client_functions)
    {
        agrpc::register_awaitable_rpc_handler<ServerRPC>(this->get_executor(), this->service, handler,
                                                         test::RethrowFirstArg{});
        perform_requests(static_cast<ClientFunctions&&>(client_functions)...);
    }

    template <class RPCHandler, class ClientFunction>
    void register_and_perform_three_requests(RPCHandler&& handler, ClientFunction&& client_function)
    {
        register_and_perform_requests(std::forward<RPCHandler>(handler), client_function, client_function,
                                      client_function);
    }
};

TEST_CASE_TEMPLATE("Awaitable ServerRPC unary success", RPC, test::UnaryServerRPC, test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, test::msg::Request& request) -> asio::awaitable<void>
        {
            CHECK_EQ(42, request.integer());
            if (use_finish_with_error)
            {
                CHECK(co_await rpc.finish_with_error(test::create_already_exists_status(), asio::use_awaitable));
            }
            else
            {
                typename RPC::Response response;
                response.set_integer(21);
                CHECK(co_await rpc.finish(response, grpc::Status::OK, asio::use_awaitable));
            }
        },
        [&](auto&, auto&, const asio::yield_context& yield)
        {
            test::client_perform_unary_success(test.grpc_context, *test.stub, yield, {use_finish_with_error});
        });
}

TEST_CASE_TEMPLATE("Awaitable unary ClientRPC/ServerRPC read/send_initial_metadata successfully", RPC,
                   test::UnaryServerRPC, test::NotifyWhenDoneUnaryServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    test.register_and_perform_three_requests(
        [&](RPC& rpc, test::msg::Request&) -> asio::awaitable<void>
        {
            rpc.context().AddInitialMetadata("test", "a");
            CHECK(co_await rpc.send_initial_metadata(asio::use_awaitable));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            grpc::ClientContext client_context;
            test::set_default_deadline(client_context);
            CHECK_EQ(grpc::StatusCode::CANCELLED,
                     test.request_rpc(client_context, request, response, yield).error_code());
            CHECK_EQ(0, client_context.GetServerInitialMetadata().find("test")->second.compare("a"));
        });
}

TEST_CASE_TEMPLATE("Awaitable streaming ClientRPC/ServerRPC read/send_initial_metadata successfully", RPC,
                   test::ClientStreamingServerRPC, test::NotifyWhenDoneClientStreamingServerRPC,
                   test::ServerStreamingServerRPC, test::NotifyWhenDoneServerStreamingServerRPC,
                   test::BidirectionalStreamingServerRPC, test::NotifyWhenDoneBidirectionalStreamingServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    test.register_and_perform_three_requests(
        [&](RPC& rpc, auto&&...) -> asio::awaitable<void>
        {
            rpc.context().AddInitialMetadata("test", "a");
            CHECK_EQ(true, co_await rpc.send_initial_metadata(asio::use_awaitable));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            CHECK(test.start_rpc(rpc, request, response, yield));
            CHECK(rpc.read_initial_metadata(yield));
            CHECK_EQ(0, rpc.context().GetServerInitialMetadata().find("test")->second.compare("a"));
        });
}

TEST_CASE_TEMPLATE("Awaitable ServerRPC/ClientRPC client streaming success", RPC, test::ClientStreamingServerRPC,
                   test::NotifyWhenDoneClientStreamingServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    bool use_finish_with_error{};
    SUBCASE("finish") {}
    SUBCASE("finish_with_error") { use_finish_with_error = true; }
    bool set_last_message{};
    SUBCASE("no last_message") {}
    SUBCASE("last_message") { set_last_message = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc) -> asio::awaitable<void>
        {
            typename RPC::Request request;
            CHECK(co_await rpc.read(request, asio::use_awaitable));
            CHECK_EQ(1, request.integer());
            CHECK(co_await rpc.read(request, asio::use_awaitable));
            CHECK_EQ(2, request.integer());
            CHECK_FALSE(co_await rpc.read(request, asio::use_awaitable));
            typename RPC::Response response;
            response.set_integer(11);
            if (use_finish_with_error)
            {
                CHECK(co_await rpc.finish_with_error(test::create_already_exists_status(), asio::use_awaitable));
            }
            else
            {
                CHECK(co_await rpc.finish(response, grpc::Status::OK, asio::use_awaitable));
            }
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

TEST_CASE_TEMPLATE("Awaitable ServerRPC/ClientRPC server streaming success", RPC, test::ServerStreamingServerRPC,
                   test::NotifyWhenDoneServerStreamingServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc, test::msg::Request& request) -> asio::awaitable<void>
        {
            CHECK_EQ(1, request.integer());
            typename RPC::Response response;
            response.set_integer(11);
            CHECK(co_await rpc.write(response, grpc::WriteOptions{}, asio::use_awaitable));
            response.set_integer(12);
            if (use_write_and_finish)
            {
                CHECK(co_await rpc.write_and_finish(response, grpc::Status::OK, asio::use_awaitable));
            }
            else
            {
                CHECK(co_await rpc.write(response, asio::use_awaitable));
                CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable));
            }
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

template <class RPC>
auto just_finish(ServerRPCAwaitableTest<RPC>& test, grpc::StatusCode expected_code = grpc::StatusCode::OK,
                 std::chrono::system_clock::time_point deadline = test::five_seconds_from_now())
{
    return [&, expected_code, deadline](auto& request, auto& response, const asio::yield_context& yield)
    {
        auto rpc = test.create_rpc();
        rpc.context().set_deadline(deadline);
        test.start_rpc(rpc, request, response, yield);
        CHECK_EQ(expected_code, rpc.finish(yield).error_code());
    };
}

TEST_CASE_FIXTURE(ServerRPCAwaitableTest<test::ServerStreamingServerRPC>,
                  "Awaitable ServerRPC/ClientRPC server streaming customize allocator")
{
    agrpc::register_awaitable_rpc_handler<ServerRPC>(
        get_executor(), service,
        [&](test::ServerStreamingServerRPC& rpc, test::msg::Request&) -> asio::awaitable<void>
        {
            CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable));
        },
        agrpc::bind_allocator(get_allocator(), test::RethrowFirstArg{}));
    const auto bytes_allocated = resource.bytes_allocated;
    perform_requests(just_finish(*this), just_finish(*this));
    CHECK_LT(bytes_allocated, resource.bytes_allocated);
}

TEST_CASE_FIXTURE(ServerRPCAwaitableTest<test::ServerStreamingServerRPC>,
                  "Awaitable ServerRPC/ClientRPC server streaming throw exception from rpc handler")
{
    std::exception_ptr eptr;
    agrpc::register_awaitable_rpc_handler<ServerRPC>(
        get_executor(), service,
        [&](test::ServerStreamingServerRPC&, test::msg::Request&) -> asio::awaitable<void>
        {
            throw test::Exception{};
            co_return;
        },
        [&](std::exception_ptr error)
        {
            eptr = error;
        });
    perform_requests_in_order(
        just_finish(*this, grpc::StatusCode::CANCELLED), just_finish(*this, grpc::StatusCode::CANCELLED),
        just_finish(*this, grpc::StatusCode::DEADLINE_EXCEEDED, test::two_hundred_milliseconds_from_now()));
    CHECK_THROWS_AS(std::rethrow_exception(eptr), test::Exception);
}

struct ServerRPCAwaitableIoContextTest : ServerRPCAwaitableTest<test::ServerStreamingServerRPC>, test::IoContextTest
{
};

TEST_CASE_FIXTURE(ServerRPCAwaitableIoContextTest,
                  "Awaitable ServerRPC/ClientRPC server streaming using io_context executor")
{
    agrpc::register_awaitable_rpc_handler<ServerRPC>(
        get_executor(), service,
        [&](test::ServerStreamingServerRPC& rpc,
            test::msg::Request&) -> asio::awaitable<void, asio::io_context::executor_type>
        {
            CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable_t<asio::io_context::executor_type>{}));
        },
        asio::bind_executor(io_context, test::RethrowFirstArg{}));
    run_io_context_detached(false);
    perform_requests(just_finish(*this), just_finish(*this));
}

TEST_CASE_TEMPLATE("Awaitable ServerRPC/ClientRPC server streaming no finish causes cancellation", RPC,
                   test::ServerStreamingServerRPC, test::NotifyWhenDoneServerStreamingServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    test.register_and_perform_three_requests(
        [&](RPC& rpc, typename RPC::Request&) -> asio::awaitable<void>
        {
            typename RPC::Response response;
            CHECK(co_await rpc.write(response, asio::use_awaitable));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            test.start_rpc(rpc, request, response, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_TEMPLATE("Awaitable ServerRPC/ClientRPC bidi streaming success", RPC, test::BidirectionalStreamingServerRPC,
                   test::NotifyWhenDoneBidirectionalStreamingServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc) -> asio::awaitable<void>
        {
            typename RPC::Request request;
            CHECK(co_await rpc.read(request, asio::use_awaitable));
            CHECK_EQ(1, request.integer());
            CHECK_FALSE(co_await rpc.read(request, asio::use_awaitable));
            typename RPC::Response response;
            response.set_integer(11);
            CHECK(co_await rpc.write(response, grpc::WriteOptions{}, asio::use_awaitable));
            response.set_integer(12);
            if (use_write_and_finish)
            {
                CHECK(co_await rpc.write_and_finish(response, grpc::Status::OK, asio::use_awaitable));
            }
            else
            {
                CHECK(co_await rpc.write(response, asio::use_awaitable));
                CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable));
            }
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

TEST_CASE_FIXTURE(ServerRPCAwaitableTest<test::GenericServerRPC>,
                  "Awaitable ServerRPC/ClientRPC generic unary RPC success")
{
    bool use_executor_overload{};
    SUBCASE("executor overload") {}
    SUBCASE("GrpcContext overload") { use_executor_overload = true; }
    register_and_perform_three_requests(
        [&](test::GenericServerRPC& rpc) -> asio::awaitable<void>
        {
            grpc::ByteBuffer request;
            CHECK(co_await rpc.read(request, asio::use_awaitable));
            CHECK_EQ(1, test::grpc_buffer_to_message<test::msg::Request>(request).integer());
            test::msg::Response response;
            response.set_integer(11);
            CHECK(co_await rpc.write_and_finish(test::message_to_grpc_buffer(response), grpc::Status::OK,
                                                asio::use_awaitable));
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
                if (use_executor_overload)
                {
                    return test::GenericUnaryClientRPC::request(get_executor(), "/test.v1.Test/Unary", *stub,
                                                                client_context, request, response, yield);
                }
                return test::GenericUnaryClientRPC::request(grpc_context, "/test.v1.Test/Unary", *stub, client_context,
                                                            request, response, yield);
            }();
            CHECK_EQ(grpc::StatusCode::OK, status.error_code());
            CHECK_EQ(11, test::grpc_buffer_to_message<test::msg::Response>(response).integer());
        });
}

TEST_CASE_TEMPLATE("Awaitable ServerRPC/ClientRPC generic streaming success", RPC, test::GenericServerRPC,
                   test::NotifyWhenDoneGenericServerRPC)
{
    ServerRPCAwaitableTest<RPC> test{};
    bool use_write_and_finish{};
    SUBCASE("finish") {}
    SUBCASE("write_and_finish") { use_write_and_finish = true; }
    test.register_and_perform_three_requests(
        [&](RPC& rpc) -> asio::awaitable<void>
        {
            typename RPC::Request request;
            CHECK(co_await rpc.read(request, asio::use_awaitable));
            CHECK_FALSE(co_await rpc.read(request, asio::use_awaitable));
            CHECK_EQ(42, test::grpc_buffer_to_message<test::msg::Request>(request).integer());
            test::msg::Response response;
            response.set_integer(21);
            if (use_write_and_finish)
            {
                typename RPC::Response raw_response = test::message_to_grpc_buffer(response);
                CHECK(co_await rpc.write_and_finish(raw_response, grpc::Status::OK, asio::use_awaitable));
            }
            else
            {
                CHECK(co_await rpc.write(test::message_to_grpc_buffer(response), asio::use_awaitable));
                CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable));
            }
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

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(ServerRPCAwaitableTest<test::BidirectionalStreamingServerRPC>,
                  "Awaitable ServerRPC resumable read can be cancelled")
{
    using RPC = test::BidirectionalStreamingServerRPC;
    register_and_perform_three_requests(
        [&](RPC& rpc) -> asio::awaitable<void>
        {
            typename RPC::Request request;
            agrpc::Waiter<void(bool)> waiter;

            waiter.initiate(agrpc::read, rpc, request);
            CHECK_EQ(true, co_await waiter.wait(asio::use_awaitable));
            CHECK_EQ(1, request.integer());
            CHECK_EQ(true, co_await waiter.wait(asio::use_awaitable));
            CHECK_EQ(1, request.integer());

            const auto not_to_exceed = test::two_hundred_milliseconds_from_now();
            waiter.initiate(agrpc::read, rpc, request);
            for (int i{}; i != 2; ++i)
            {
                auto [completion_order, ec, read_ok] =
                    co_await asio::experimental::make_parallel_group(
                        waiter.wait(test::ASIO_DEFERRED),
                        asio::post(asio::bind_executor(grpc_context, test::ASIO_DEFERRED)))
                        .async_wait(asio::experimental::wait_for_one(), asio::use_awaitable);
                CHECK_LT(test::now(), not_to_exceed);
                CHECK_EQ(asio::error::operation_aborted, ec);
                CHECK_EQ(1, request.integer());
            }
            CHECK_EQ(false, co_await waiter.wait(asio::use_awaitable));
            CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable));
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, request, response, yield);
            request.set_integer(1);
            CHECK(rpc.write(request, yield));
            agrpc::Waiter<void(bool)> waiter;
            waiter.initiate(agrpc::read, rpc, response);
            auto [completion_order, ec, read_ok, wait_ok, alarm] =
                asio::experimental::make_parallel_group(
                    waiter.wait(test::ASIO_DEFERRED),
                    agrpc::Alarm(grpc_context).wait(test::five_hundred_milliseconds_from_now(), test::ASIO_DEFERRED))
                    .async_wait(asio::experimental::wait_for_one(), yield);
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
            waiter.wait(yield);
        });
}

TEST_CASE_FIXTURE(ServerRPCAwaitableTest<test::ServerStreamingServerRPC>,
                  "Awaitable ServerRPC/ClientRPC server streaming cancel register_awaitable_rpc_handler")
{
    asio::cancellation_signal signal;
    std::exception_ptr eptr;
    agrpc::register_awaitable_rpc_handler<ServerRPC>(
        get_executor(), service,
        [&](test::ServerStreamingServerRPC& rpc, test::msg::Request&) -> asio::awaitable<void>
        {
            CHECK(co_await rpc.finish(grpc::Status::OK, asio::use_awaitable));
        },
        asio::bind_cancellation_slot(signal.slot(),
                                     [&](std::exception_ptr error)
                                     {
                                         eptr = error;
                                     }));
    signal.emit(asio::cancellation_type::total);
    perform_requests_in_order(just_finish(*this), just_finish(*this, grpc::StatusCode::DEADLINE_EXCEEDED,
                                                              test::two_hundred_milliseconds_from_now()));
    CHECK_FALSE(eptr);
}
#endif
#endif