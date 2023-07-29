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
#include "utils/delete_guard.hpp"
#include "utils/doctest.hpp"
#include "utils/exception.hpp"
#include "utils/inline_executor.hpp"
#include "utils/io_context_test.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <agrpc/notify_when_done.hpp>
#include <agrpc/wait.hpp>
#include <grpcpp/grpcpp.h>

#include <future>

template <class RPC>
struct ClientRPCIoContextTest : test::ClientRPCTest<RPC>, test::IoContextTest
{
    void run_server_client_on_separate_threads(std::function<void(const asio::yield_context&)> server_func,
                                               std::function<void(const asio::yield_context&)> client_func)
    {
        test::typed_spawn(io_context,
                          [client_func, g = this->get_work_tracking_executor()](const asio::yield_context& yield)
                          {
                              client_func(yield);
                          });
        this->run_io_context_detached(false);
        this->spawn_and_run(
            [server_func](const asio::yield_context& yield)
            {
                server_func(yield);
            });
    }
};

TEST_CASE_TEMPLATE("Streaming RPC can be destructed without being started", RPC, test::ClientStreamingClientRPC,
                   test::ClientStreamingInterfaceClientRPC, test::ServerStreamingClientRPC,
                   test::ServerStreamingInterfaceClientRPC, test::BidirectionalStreamingClientRPC,
                   test::BidirectionalStreamingInterfaceClientRPC, test::GenericStreamingClientRPC)
{
    agrpc::GrpcContext grpc_context;
    CHECK_NOTHROW([[maybe_unused]] RPC rpc{grpc_context.get_executor()});
}

TEST_CASE_TEMPLATE("Unary RPC::request automatically finishes RPC on error", RPC, test::UnaryClientRPC,
                   test::UnaryInterfaceClientRPC, test::GenericUnaryClientRPC)
{
    test::ClientRPCTest<RPC> test;
    bool use_executor_overload{};
    SUBCASE("executor overload") {}
    SUBCASE("GrpcContext overload") { use_executor_overload = true; }
    test.server->Shutdown();
    test.client_context.set_deadline(test::ten_milliseconds_from_now());
    test.request_rpc(use_executor_overload,
                     [](const grpc::Status& status)
                     {
                         const auto status_code = status.error_code();
                         CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == status_code ||
                                        grpc::StatusCode::UNAVAILABLE == status_code),
                                       status_code);
                     });
    test.grpc_context.run();
}

TEST_CASE_TEMPLATE("Streaming RPC::start returns false on error", RPC, test::ClientStreamingClientRPC,
                   test::ClientStreamingInterfaceClientRPC, test::ServerStreamingClientRPC,
                   test::ServerStreamingInterfaceClientRPC, test::BidirectionalStreamingClientRPC,
                   test::BidirectionalStreamingInterfaceClientRPC, test::GenericStreamingClientRPC)
{
    test::ClientRPCTest<RPC> test;
    test.server->Shutdown();
    RPC rpc{test.get_executor()};
    rpc.context().set_deadline(test::ten_milliseconds_from_now());
    test.start_rpc(rpc,
                   [&](bool ok)
                   {
                       CHECK_FALSE(ok);
                       rpc.finish(
                           [](grpc::Status&& status)
                           {
                               const auto status_code = status.error_code();
                               CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == status_code ||
                                              grpc::StatusCode::UNAVAILABLE == status_code),
                                             status_code);
                           });
                   });
    test.grpc_context.run();
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ServerStreamingClientRPC>,
                  "UnaryClientRPC::request exception thrown from completion handler rethrows from GrpcContext.run()")
{
    CHECK_THROWS_AS(spawn_and_run(
                        [&](const asio::yield_context& yield)
                        {
                            test_server.request_rpc(yield);
                            agrpc::finish(test_server.responder, grpc::Status::OK, yield);
                        },
                        [&](const asio::yield_context& yield)
                        {
                            auto rpc = std::make_unique<test::ServerStreamingClientRPC>(grpc_context,
                                                                                        test::set_default_deadline);
                            start_rpc(*rpc, yield);
                            auto& r = *rpc;
                            r.read(response, asio::bind_executor(test::InlineExecutor{},
                                                                 [r = std::move(rpc)](bool)
                                                                 {
                                                                     throw test::Exception{};
                                                                 }));
                        }),
                    test::Exception);
}

TEST_CASE_TEMPLATE("ClientRPC::read_initial_metadata successfully", RPC, test::ClientStreamingClientRPC,
                   test::ServerStreamingClientRPC, test::BidirectionalStreamingClientRPC)
{
    test::ClientRPCTest<RPC> test;
    test.spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test.test_server.request_rpc(yield);
            agrpc::send_initial_metadata(test.test_server.responder, yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            CHECK(test.start_rpc(rpc, yield));
            CHECK(rpc.read_initial_metadata(yield));
        });
}

TEST_CASE_TEMPLATE("ClientRPC::read_initial_metadata on cancelled RPC", RPC, test::ClientStreamingClientRPC,
                   test::ServerStreamingClientRPC)
{
    test::ClientRPCTest<RPC> test;
    test.spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test.server_request_rpc_and_cancel(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            CHECK(test.start_rpc(rpc, yield));
            rpc.cancel();
            CHECK_FALSE(rpc.read_initial_metadata(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
            test.server_shutdown.initiate();
        });
}

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
TEST_CASE_FIXTURE(test::ClientRPCTest<test::UnaryClientRPC>,
                  "ClientRPC::request can have UseSender as default completion token")
{
    using RPC = agrpc::UseSender::as_default_on_t<agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncUnary>>;
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
            const test::FunctionAsReceiver receiver{[&](grpc::Status&& status)
                                                    {
                                                        ok = status.ok();
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
#endif

TEST_CASE_FIXTURE(test::ClientRPCTest<test::GenericUnaryClientRPC>, "ClientRPC::request generic unary RPC successfully")
{
    bool use_executor_overload{};
    SUBCASE("executor overload") {}
    SUBCASE("GrpcContext overload") { use_executor_overload = true; }
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
            test::msg::Request typed_request;
            typed_request.set_integer(42);
            request = test::message_to_grpc_buffer(typed_request);
            auto status = request_rpc(use_executor_overload, yield);
            CHECK(status.ok());
            CHECK_EQ(24, test::grpc_buffer_to_message<test::msg::Response>(response).integer());
        });
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ServerStreamingClientRPC>, "ServerStreamingClientRPC::read successfully")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK_EQ(42, test_server.request.integer());
            test_server.response.set_integer(1);
            CHECK(agrpc::write(test_server.responder, test_server.response, yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            request.set_integer(42);
            start_rpc(rpc, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(1, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ServerStreamingClientRPC>, "ServerStreamingClientRPC::read failure")
{
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            server_request_rpc_and_cancel(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            rpc.cancel();
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
            server_shutdown.initiate();
        });
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ServerStreamingClientRPC>,
                  "ServerStreamingClientRPC can handle cancellation")
{
    bool explicit_cancellation{};
    SUBCASE("automatic cancellation on destruction") {}
    SUBCASE("explicit cancellation") { explicit_cancellation = true; }
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            server_request_rpc_and_cancel(yield);
        },
        [&](const asio::yield_context& yield)
        {
            {
                auto rpc = create_rpc();
                start_rpc(rpc, yield);
                if (explicit_cancellation)
                {
                    rpc.cancel();
                }
            }
            server_shutdown.initiate();
        });
}

auto create_is_cancelled_future(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context)
{
    std::promise<bool> is_cancelled_promise;
    auto future = is_cancelled_promise.get_future();
    agrpc::notify_when_done(grpc_context, server_context,
                            [&, promise = std::move(is_cancelled_promise)]() mutable
                            {
                                promise.set_value(server_context.IsCancelled());
                            });
    return future;
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::ClientStreamingClientRPC>,
                  "ClientStreamingClientRPC automatically cancels on destruction")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            auto is_cancelled_future = create_is_cancelled_future(grpc_context, server_context);
            CHECK(test_server.request_rpc(yield));
            agrpc::read(test_server.responder, test_server.request, yield);

            // start and finish second request
            grpc::ServerContext new_server_context;
            grpc::ServerAsyncReader<test::msg::Response, test::msg::Request> responder{&new_server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, test_server.service,
                                 new_server_context, responder, yield));
            test_server.response.set_integer(42);
            CHECK(agrpc::finish(responder, test_server.response, grpc::Status::OK, yield));

            // wait for cancellation signal from first request
            grpc::Alarm alarm;
            for (int i{}; i < 50; ++i)
            {
                agrpc::wait(alarm, test::ten_milliseconds_from_now(), yield);
                if (is_cancelled_future.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                {
                    CHECK(is_cancelled_future.get());
                    return;
                }
            }
            FAIL("timeout reached while waiting for cancellation signal");
        },
        [&](const asio::yield_context& yield)
        {
            {
                auto rpc = create_rpc();
                start_rpc(rpc, yield);
                rpc.write(request, yield);
            }
            {
                auto rpc = create_rpc();
                CHECK(start_rpc(rpc, yield));
                CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
                CHECK_EQ(42, response.integer());
            }
        });
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ClientStreamingClientRPC>, "ClientStreamingClientRPC::write successfully")
{
    bool set_last_message{};
    SUBCASE("write and finish separately") {}
    SUBCASE("set_last_message") { set_last_message = true; }
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            test_server.response.set_integer(1);
            CHECK_FALSE(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK(agrpc::finish(test_server.responder, test_server.response, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            request.set_integer(42);
            if (set_last_message)
            {
                CHECK(rpc.write(request, grpc::WriteOptions{}.set_last_message(), yield));
            }
            else
            {
                CHECK(rpc.write(request, yield));
            }
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ClientStreamingClientRPC>, "ClientStreamingClientRPC::write failure")
{
    grpc::WriteOptions options{};
    SUBCASE("") {}
    SUBCASE("set_last_message") { options.set_last_message(); }
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            server_request_rpc_and_cancel(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            rpc.cancel();
            CHECK_FALSE(rpc.write(request, options, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
            server_shutdown.initiate();
        });
}

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
TEST_CASE_FIXTURE(test::ClientRPCTest<test::ClientStreamingClientRPC>, "ClientStreamingClientRPC::finish using sender")
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
            else
            {
                server_context.TryCancel();
            }
        },
        [&](asio::yield_context yield)
        {
            auto rpc = std::make_unique<test::ClientStreamingClientRPC>(grpc_context, test::set_default_deadline);
            start_rpc(*rpc, yield);
            if (!expected_ok)
            {
                rpc->cancel();
            }
            auto& rpc_ref = *rpc;
            asio::execution::submit(rpc_ref.finish(agrpc::use_sender),
                                    test::FunctionAsReceiver{[&, rpc = std::move(rpc)](grpc::Status status) mutable
                                                             {
                                                                 CHECK_EQ(expected_status_code, status.error_code());
                                                             }});
        });
}
#endif

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::BidirectionalStreamingClientRPC>,
                  "BidirectionalStreamingClientRPC success")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            test_server.response.set_integer(1);
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_FALSE(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            CHECK(agrpc::write(test_server.responder, test_server.response, yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            request.set_integer(42);
            CHECK(rpc.write(request, yield));
            CHECK(rpc.writes_done(yield));
            CHECK(rpc.read(response, yield));
            CHECK_EQ(1, response.integer());
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(1, response.integer());
            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::BidirectionalStreamingClientRPC>,
                  "BidirectionalStreamingClientRPC concurrent read+write")
{
    bool set_last_message{};
    SUBCASE("no WriteOptions") {}
    SUBCASE("set_last_message") { set_last_message = true; }
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            CHECK(agrpc::write(test_server.responder, test_server.response, {}, yield));
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status{grpc::StatusCode::ALREADY_EXISTS, ""}, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            CHECK(rpc.read(response, yield));
            std::promise<bool> promise;
            const auto fulfill_promise = [&](bool ok)
            {
                promise.set_value(ok);
            };
            if (set_last_message)
            {
                grpc::WriteOptions options{};
                rpc.write(request, options.set_last_message(), fulfill_promise);
            }
            else
            {
                rpc.write(request, fulfill_promise);
            }
            CHECK_FALSE(rpc.read(response, yield));
            CHECK(promise.get_future().get());
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::BidirectionalStreamingClientRPC>,
                  "BidirectionalStreamingClientRPC cancel before write+read")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            agrpc::finish(test_server.responder, grpc::Status::OK, yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            rpc.cancel();
            std::promise<bool> promise;
            rpc.read(response,
                     [&](bool ok)
                     {
                         promise.set_value(ok);
                     });
            CHECK_FALSE(rpc.write(request, yield));
            CHECK_FALSE(promise.get_future().get());
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::GenericStreamingClientRPC>, "GenericStreamingClientRPC success")
{
    run_server_client_on_separate_threads(
        [&](const asio::yield_context& yield)
        {
            CHECK(test_server.request_rpc(yield));
            test_server.response.set_integer(1);
            CHECK(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_FALSE(agrpc::read(test_server.responder, test_server.request, yield));
            CHECK_EQ(42, test_server.request.integer());
            CHECK(agrpc::write(test_server.responder, test_server.response, yield));
            CHECK(agrpc::finish(test_server.responder, grpc::Status::OK, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            CHECK(start_rpc(rpc, yield));

            test::msg::Request typed_request;
            typed_request.set_integer(42);
            CHECK(rpc.write(test::message_to_grpc_buffer(typed_request), yield));
            CHECK(rpc.writes_done(yield));

            CHECK(rpc.read(response, yield));
            CHECK_EQ(1, test::grpc_buffer_to_message<test::msg::Response>(response).integer());

            response.Clear();
            CHECK_FALSE(rpc.read(response, yield));

            CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
        });
}

TEST_CASE("ClientRPC::service_name/method_name")
{
    const auto check_eq_and_null_terminated = [](std::string_view expected, std::string_view actual)
    {
        CHECK_EQ(expected, actual);
        CHECK_EQ('\0', *(actual.data() + actual.size()));
    };
    check_eq_and_null_terminated("test.v1.Test", test::UnaryClientRPC::service_name());
    check_eq_and_null_terminated("Unary", test::UnaryClientRPC::method_name());
    check_eq_and_null_terminated("test.v1.Test", test::ClientStreamingClientRPC::service_name());
    check_eq_and_null_terminated("ClientStreaming", test::ClientStreamingClientRPC::method_name());
    check_eq_and_null_terminated("test.v1.Test", test::ServerStreamingClientRPC::service_name());
    check_eq_and_null_terminated("ServerStreaming", test::ServerStreamingClientRPC::method_name());
    check_eq_and_null_terminated("test.v1.Test", test::BidirectionalStreamingClientRPC::service_name());
    check_eq_and_null_terminated("BidirectionalStreaming", test::BidirectionalStreamingClientRPC::method_name());
}

struct Derived : test::ServerStreamingClientRPC
{
    template <class T = test::ServerStreamingClientRPC>
    auto is_finished(int) -> decltype((void)T::is_finished(), std::true_type{});

    template <class T = test::ServerStreamingClientRPC>
    auto is_finished(long) -> std::false_type;
};

TEST_CASE("ClientRPC derived class cannot access private base member")
{
    CHECK_FALSE(decltype(std::declval<Derived>().is_finished(0))::value);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
template <class RPC>
struct ClientRPCCancellationTest : test::ClientRPCTest<RPC>, test::IoContextTest
{
    asio::steady_timer timer{io_context};

    ClientRPCCancellationTest() { run_io_context_detached(); }
};

// gRPC requests seem to be uncancellable on platforms other than Windows
#ifdef _WIN32
TEST_CASE_TEMPLATE("Unary RPC::request can be cancelled", RPC, test::UnaryClientRPC, test::GenericUnaryClientRPC)
{
    ClientRPCCancellationTest<RPC> test;
    test.server->Shutdown();
    const auto not_to_exceed = test::one_second_from_now();
    asio::experimental::make_parallel_group(test.request_rpc(test::ASIO_DEFERRED),
                                            asio::post(asio::bind_executor(test.grpc_context, test::ASIO_DEFERRED)))
        .async_wait(asio::experimental::wait_for_one(),
                    [&](auto&&, const grpc::Status& status, auto&&...)
                    {
                        CHECK_FALSE(status.ok());
                        CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
                    });
    test.grpc_context.run();
    CHECK_LT(test::now(), not_to_exceed);
}

TEST_CASE_TEMPLATE("Streaming RPC::start can be cancelled", RPC, test::ClientStreamingClientRPC,
                   test::ServerStreamingClientRPC, test::BidirectionalStreamingClientRPC,
                   test::GenericStreamingClientRPC)
{
    ClientRPCCancellationTest<RPC> test;
    test.server->Shutdown();
    const auto not_to_exceed = test::one_second_from_now();
    auto rpc = test.create_rpc();
    asio::experimental::make_parallel_group(test.start_rpc(rpc, test::ASIO_DEFERRED),
                                            asio::post(asio::bind_executor(test.grpc_context, test::ASIO_DEFERRED)))
        .async_wait(asio::experimental::wait_for_one(),
                    [&](auto&&, bool ok, auto&&...)
                    {
                        CHECK_FALSE(ok);
                        rpc.finish(
                            [&](grpc::Status&& status)
                            {
                                CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
                            });
                    });
    test.grpc_context.run();
    CHECK_LT(test::now(), not_to_exceed);
}
#endif

template <class RPCType>
struct StreamingReadInitialMetadataCancellationT
{
    using RPC = RPCType;

    static auto step(ClientRPCCancellationTest<RPC>&, RPC& rpc)
    {
        return rpc.read_initial_metadata(test::ASIO_DEFERRED);
    }
};

using ClientStreamingReadInitialMetadataCancellation =
    StreamingReadInitialMetadataCancellationT<test::ClientStreamingClientRPC>;
TYPE_TO_STRING(ClientStreamingReadInitialMetadataCancellation);

using ServerStreamingReadInitialMetadataCancellation =
    StreamingReadInitialMetadataCancellationT<test::ServerStreamingClientRPC>;
TYPE_TO_STRING(ServerStreamingReadInitialMetadataCancellation);

using BidiStreamingReadInitialMetadataCancellation =
    StreamingReadInitialMetadataCancellationT<test::BidirectionalStreamingClientRPC>;
TYPE_TO_STRING(BidiStreamingReadInitialMetadataCancellation);

using GenericBidiStreamingReadInitialMetadataCancellation =
    StreamingReadInitialMetadataCancellationT<test::GenericStreamingClientRPC>;
TYPE_TO_STRING(GenericBidiStreamingReadInitialMetadataCancellation);

template <class RPCType>
struct StreamingReadCancellationT
{
    using RPC = RPCType;

    static auto step(ClientRPCCancellationTest<RPC>& test, RPC& rpc)
    {
        return rpc.read(test.response, test::ASIO_DEFERRED);
    }
};

using ServerStreamingReadCancellation = StreamingReadCancellationT<test::ServerStreamingClientRPC>;
TYPE_TO_STRING(ServerStreamingReadCancellation);

using BidiStreamingReadCancellation = StreamingReadCancellationT<test::BidirectionalStreamingClientRPC>;
TYPE_TO_STRING(BidiStreamingReadCancellation);

using GenericBidiStreamingReadCancellation = StreamingReadCancellationT<test::GenericStreamingClientRPC>;
TYPE_TO_STRING(GenericBidiStreamingReadCancellation);

template <class RPCType>
struct StreamingFinishCancellationT
{
    using RPC = RPCType;

    static auto step(ClientRPCCancellationTest<RPC>&, RPC& rpc) { return rpc.finish(test::ASIO_DEFERRED); }
};

using ClientStreamingFinishCancellation = StreamingFinishCancellationT<test::ClientStreamingClientRPC>;
TYPE_TO_STRING(ClientStreamingFinishCancellation);

using ServerStreamingFinishCancellation = StreamingFinishCancellationT<test::ServerStreamingClientRPC>;
TYPE_TO_STRING(ServerStreamingFinishCancellation);

using BidiStreamingFinishCancellation = StreamingFinishCancellationT<test::BidirectionalStreamingClientRPC>;
TYPE_TO_STRING(BidiStreamingFinishCancellation);

using GenericBidiStreamingFinishCancellation = StreamingFinishCancellationT<test::GenericStreamingClientRPC>;
TYPE_TO_STRING(GenericBidiStreamingFinishCancellation);

template <class T>
void test_rpc_step_functions_can_be_cancelled()
{
    ClientRPCCancellationTest<typename T::RPC> test;
    const auto not_to_exceed = test::one_second_from_now();
    test.spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            test.test_server.request_rpc(yield);
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            test.start_rpc(rpc, yield);
            [[maybe_unused]] auto result =
                asio::experimental::make_parallel_group(
                    asio::post(asio::bind_executor(test.grpc_context, test::ASIO_DEFERRED)), T::step(test, rpc))
                    .async_wait(asio::experimental::wait_for_one(), yield);
            if constexpr (std::is_same_v<grpc::Status&, decltype(std::get<1>(result))>)
            {
                CHECK_EQ(grpc::StatusCode::CANCELLED, std::get<1>(result).error_code());
            }
            else
            {
                CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
            }
            test.server_shutdown.initiate();
        });
    CHECK_LT(test::now(), not_to_exceed);
}

TEST_CASE_TEMPLATE("ClientRPC::read_initial_metadata can be cancelled", T,
                   ClientStreamingReadInitialMetadataCancellation, ServerStreamingReadInitialMetadataCancellation,
                   BidiStreamingReadInitialMetadataCancellation, GenericBidiStreamingReadInitialMetadataCancellation)
{
    if (grpc::Version() > "1.20.0")
    {
        test_rpc_step_functions_can_be_cancelled<T>();
    }
}

TEST_CASE_TEMPLATE("RPC step functions can be cancelled", T, ServerStreamingReadCancellation,
                   BidiStreamingReadCancellation, GenericBidiStreamingReadCancellation,
                   ClientStreamingFinishCancellation, ServerStreamingFinishCancellation,
                   BidiStreamingFinishCancellation, GenericBidiStreamingFinishCancellation)
{
    test_rpc_step_functions_can_be_cancelled<T>();
}
#endif