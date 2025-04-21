// Copyright 2024 Dennis Hezel
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

#include "utils/asio_utils.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/delete_guard.hpp"
#include "utils/doctest.hpp"
#include "utils/exception.hpp"
#include "utils/io_context_test.hpp"
#include "utils/server_rpc.hpp"
#include "utils/time.hpp"
#include "utils/utility.hpp"

#include <agrpc/client_rpc.hpp>
#include <grpcpp/grpcpp.h>

template <class RPC>
struct ClientRPCTest : test::ClientServerRPCTest<RPC>
{
    auto create_rpc() { return RPC{this->grpc_context, test::set_default_deadline}; }
};

template <class RPC>
struct ClientRPCRequestResponseTest : ClientRPCTest<RPC>
{
    using ClientRPCTest<RPC>::start_rpc;

    template <class CompletionToken>
    auto start_rpc(RPC& rpc, CompletionToken&& token)
    {
        return test::ClientServerRPCTest<RPC>::start_rpc(rpc, this->request, this->response,
                                                         static_cast<CompletionToken&&>(token));
    }

    using ClientRPCTest<RPC>::request_rpc;

    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return test::ClientServerRPCTest<RPC>::request_rpc(this->client_context, this->request, this->response,
                                                           static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken>
    auto request_rpc(bool use_executor, CompletionToken&& token)
    {
        return test::ClientServerRPCTest<RPC>::request_rpc(use_executor, this->client_context, this->request,
                                                           this->response, static_cast<CompletionToken&&>(token));
    }

    typename RPC::Request request;
    typename RPC::Response response;
};

template <class RPC>
struct ClientRPCIoContextTest : ClientRPCRequestResponseTest<RPC>, test::IoContextTest
{
    template <class SRPC = typename ClientRPCRequestResponseTest<RPC>::ServerRPC>
    void run_server_client_on_separate_threads(
        std::function<void(test::TypeIdentityT<SRPC>&, const asio::yield_context&)> server_func,
        std::function<void(const asio::yield_context&)> client_func)
    {
        test::spawn(io_context,
                    [this, client_func, g = this->get_work_tracking_executor()](const asio::yield_context& yield)
                    {
                        client_func(yield);
                        this->server_shutdown.initiate();
                    });
        agrpc::register_yield_rpc_handler<SRPC>(this->grpc_context, this->service, server_func,
                                                test::RethrowFirstArg{});
        this->run_io_context_detached(false);
        this->grpc_context.run();
    }
};

TEST_CASE_TEMPLATE("ClientRPC::request successfully", RPC, test::UnaryClientRPC, test::UnaryInterfaceClientRPC)
{
    ClientRPCTest<RPC> test;
    test.register_and_perform_three_requests(
        [&](auto& rpc, const typename RPC::Request& request, const asio::yield_context& yield)
        {
            CHECK_EQ(42, request.integer());
            typename RPC::Response response;
            rpc.finish(response, grpc::Status::OK, yield);
        },
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            request.set_integer(42);
            grpc::ClientContext client_context;
            test::set_default_deadline(client_context);
            const auto status = test.request_rpc(false, client_context, request, response, yield);
            CHECK_EQ(grpc::StatusCode::OK, status.error_code());
        });
}

TEST_CASE_TEMPLATE("Unary ClientRPC::request automatically finishes rpc on error", RPC, test::UnaryClientRPC,
                   test::UnaryInterfaceClientRPC, test::GenericUnaryClientRPC)
{
    ClientRPCRequestResponseTest<RPC> test;
    bool use_executor_overload{};
    SUBCASE("executor overload") {}
    SUBCASE("GrpcContext overload") { use_executor_overload = true; }
    test.server->Shutdown();
    test.client_context.set_deadline(test::ten_milliseconds_from_now());
    test.request_rpc(use_executor_overload, test.client_context, test.request, test.response,
                     [](const grpc::Status& status)
                     {
                         const auto status_code = status.error_code();
                         CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == status_code ||
                                        grpc::StatusCode::UNAVAILABLE == status_code),
                                       status_code);
                     });
    test.grpc_context.run();
}

TEST_CASE_TEMPLATE("Unary ClientRPC can be destructed after start", RPC, test::UnaryClientRPC,
                   test::GenericUnaryClientRPC)
{
    ClientRPCRequestResponseTest<RPC> test;
    RPC rpc{test.get_executor()};
    test.start_rpc(rpc, int{});
}

TEST_CASE_TEMPLATE("Streaming ClientRPC can be destructed without being started", RPC, test::UnaryClientRPC,
                   test::GenericUnaryClientRPC, test::ClientStreamingClientRPC, test::ClientStreamingInterfaceClientRPC,
                   test::ServerStreamingClientRPC, test::ServerStreamingInterfaceClientRPC,
                   test::BidirectionalStreamingClientRPC, test::BidirectionalStreamingInterfaceClientRPC,
                   test::GenericStreamingClientRPC)
{
    agrpc::GrpcContext grpc_context;
    CHECK_NOTHROW([[maybe_unused]] RPC rpc{grpc_context.get_executor()});
}

TEST_CASE_TEMPLATE("Streaming ClientRPC::start returns false on error", RPC, test::ClientStreamingClientRPC,
                   test::ClientStreamingInterfaceClientRPC, test::ServerStreamingClientRPC,
                   test::ServerStreamingInterfaceClientRPC, test::BidirectionalStreamingClientRPC,
                   test::BidirectionalStreamingInterfaceClientRPC, test::GenericStreamingClientRPC)
{
    ClientRPCRequestResponseTest<RPC> test;
    test.server->Shutdown();
    RPC rpc{test.get_executor()};
    rpc.context().set_deadline(test::ten_milliseconds_from_now());
    test.start_rpc(rpc, test.request, test.response,
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

TEST_CASE_FIXTURE(
    ClientRPCRequestResponseTest<test::ServerStreamingClientRPC>,
    "ServerStreamingClientRPC::start exception thrown from completion handler rethrows from GrpcContext.run()")
{
    server->Shutdown();
    test::ServerStreamingClientRPC rpc{get_executor()};
    rpc.context().set_deadline(test::ten_milliseconds_from_now());
    start_rpc(rpc, request, response,
              asio::bind_executor(grpc_context,
                                  [&](bool)
                                  {
                                      throw test::Exception{};
                                  }));
    CHECK_THROWS_AS(grpc_context.run(), test::Exception);
}

TEST_CASE_TEMPLATE("ClientRPC::read_initial_metadata on cancelled RPC", RPC, test::ClientStreamingClientRPC,
                   test::ServerStreamingClientRPC, test::BidirectionalStreamingClientRPC)
{
    ClientRPCTest<RPC> test;
    test.run_server_immediate_cancellation(
        [&](auto& request, auto& response, const asio::yield_context& yield)
        {
            auto rpc = test.create_rpc();
            CHECK(test.start_rpc(rpc, request, response, yield));
            rpc.cancel();
            CHECK_FALSE(rpc.read_initial_metadata(yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ClientRPCRequestResponseTest<test::UnaryClientRPC>,
                  "ClientRPC::request can have UseSender as default completion token")
{
    using RPC = agrpc::UseSender::as_default_on_t<agrpc::ClientRPC<&test::v1::Test::Stub::PrepareAsyncUnary>>;
    bool ok{};
    test::DeleteGuard guard{};
    register_and_perform_requests_no_shutdown(
        [&](auto& rpc, auto& request, const asio::yield_context& yield)
        {
            CHECK_EQ(42, request.integer());
            Response response;
            response.set_integer(21);
            CHECK(rpc.finish(response, grpc::Status::OK, yield));
            server_shutdown.initiate();
        },
        [&](auto&&...)
        {
            request.set_integer(42);
            auto sender = RPC::request(grpc_context, *stub, client_context, request, response);
            const test::FunctionAsReceiver receiver{[&](grpc::Status&& status)
                                                    {
                                                        ok = status.ok();
                                                    }};
            auto& operation_state = guard.emplace_with(
                [&]
                {
                    return std::move(sender).connect(receiver);
                });
            operation_state.start();
        });
    CHECK(ok);
    CHECK_EQ(21, response.integer());
}

TEST_CASE_FIXTURE(ClientRPCTest<test::ServerStreamingClientRPC>, "ServerStreamingClientRPC::read failure")
{
    run_server_immediate_cancellation(
        [&](Request& request, Response& response, const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, request, response, yield);
            CHECK_FALSE(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ClientRPCTest<test::ServerStreamingClientRPC>, "ServerStreamingClientRPC can handle cancellation")
{
    bool explicit_cancellation{};
    SUBCASE("automatic cancellation on destruction") {}
    SUBCASE("explicit cancellation") { explicit_cancellation = true; }
    run_server_immediate_cancellation(
        [&](Request& request, Response& response, const asio::yield_context& yield)
        {
            {
                auto rpc = create_rpc();
                start_rpc(rpc, request, response, yield);
                if (explicit_cancellation)
                {
                    rpc.cancel();
                }
            }
        });
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::ClientStreamingClientRPC>,
                  "ClientStreamingClientRPC automatically cancels on destruction")
{
    bool is_first{true};
    run_server_client_on_separate_threads<test::NotifyWhenDoneClientStreamingServerRPC>(
        [&](auto& rpc, const asio::yield_context& yield)
        {
            if (std::exchange(is_first, false))
            {
                Request request;
                rpc.read(request, yield);
                rpc.wait_for_done(yield);
                CHECK(rpc.context().IsCancelled());
            }
            else
            {
                Response response;
                response.set_integer(11);
                CHECK(rpc.finish(response, grpc::Status::OK, yield));
            }
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
                Response response;
                CHECK(start_rpc(rpc, request, response, yield));
                CHECK_EQ(grpc::StatusCode::OK, rpc.finish(yield).error_code());
                CHECK_EQ(11, response.integer());
            }
        });
}

TEST_CASE_FIXTURE(ClientRPCTest<test::ClientStreamingClientRPC>, "ClientStreamingClientRPC::write failure")
{
    grpc::WriteOptions options{};
    SUBCASE("") {}
    SUBCASE("set_last_message") { options.set_last_message(); }
    run_server_immediate_cancellation(
        [&](Request& request, Response& response, const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, request, response, yield);
            rpc.cancel();
            CHECK_FALSE(rpc.write(request, options, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
        });
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::BidirectionalStreamingClientRPC>,
                  "BidirectionalStreamingClientRPC initiate write during read")
{
    bool set_last_message{};
    SUBCASE("no WriteOptions") {}
    SUBCASE("set_last_message") { set_last_message = true; }
    run_server_client_on_separate_threads(
        [&](auto& rpc, const asio::yield_context& yield)
        {
            CHECK(rpc.write({}, {}, yield));
            Request request;
            CHECK(rpc.read(request, yield));
            CHECK(rpc.finish(grpc::Status{grpc::StatusCode::ALREADY_EXISTS, ""}, yield));
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
                  "BidirectionalStreamingClientRPC initiate finish during read")
{
    run_server_client_on_separate_threads(
        [&](auto& rpc, const asio::yield_context& yield)
        {
            CHECK(rpc.finish(grpc::Status{grpc::StatusCode::ALREADY_EXISTS, ""}, yield));
        },
        [&](const asio::yield_context& yield)
        {
            auto rpc = create_rpc();
            start_rpc(rpc, yield);
            std::promise<bool> promise;
            bool read{};
            rpc.read_initial_metadata(
                [&](auto&& ok)
                {
                    read = ok;
                });
            rpc.read(response,
                     [&](bool ok)
                     {
                         promise.set_value(ok);
                     });
            CHECK_EQ(grpc::StatusCode::ALREADY_EXISTS, rpc.finish(yield).error_code());
            CHECK_FALSE(promise.get_future().get());
            CHECK(read);
        });
}

TEST_CASE_FIXTURE(ClientRPCIoContextTest<test::BidirectionalStreamingClientRPC>,
                  "BidirectionalStreamingClientRPC cancel before write+read")
{
    run_server_client_on_separate_threads(
        [&](auto& rpc, const asio::yield_context& yield)
        {
            rpc.finish(grpc::Status::OK, yield);
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
    auto grpc_context(int) -> decltype((void)&T::grpc_context, std::true_type{});

    template <class T = test::ServerStreamingClientRPC>
    auto grpc_context(long) -> std::false_type;
};

TEST_CASE("ClientRPC derived class cannot access private base member")
{
    CHECK_FALSE(decltype(std::declval<Derived>().grpc_context(0))::value);
}

#ifdef AGRPC_TEST_ASIO_PARALLEL_GROUP
// gRPC requests seem to be uncancellable on platforms other than Windows
#ifdef _WIN32
TEST_CASE_TEMPLATE("Unary RPC::request can be cancelled", RPC, test::UnaryClientRPC, test::GenericUnaryClientRPC)
{
    ClientRPCRequestResponseTest<RPC> test;
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
    ClientRPCRequestResponseTest<RPC> test;
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

    static auto step(ClientRPCRequestResponseTest<RPC>&, RPC& rpc)
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

    static auto step(ClientRPCRequestResponseTest<RPC>& test, RPC& rpc)
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

    static auto step(ClientRPCRequestResponseTest<RPC>&, RPC& rpc) { return rpc.finish(test::ASIO_DEFERRED); }
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
    ClientRPCRequestResponseTest<typename T::RPC> test;
    const auto not_to_exceed = test::two_seconds_from_now();
    test.register_and_perform_three_requests(
        [&](auto&&...) {},
        [&](auto&, auto&, const asio::yield_context& yield)
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