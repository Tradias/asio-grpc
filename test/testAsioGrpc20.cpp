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
#include "utils/asioUtils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/rpc.hpp"
#include "utils/time.hpp"

#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>

#include <cstddef>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_TEMPLATE("awaitable server streaming", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    constexpr bool IS_STUB_INTERFACE = std::is_same_v<test::v1::Test::StubInterface, Stub>;
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    test::co_spawn_and_run(
        test.grpc_context,
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            grpc::ServerAsyncWriter<test::msg::Response> writer{&test.server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, test.service,
                                          test.server_context, request, writer));
            test::ServerAsyncWriter<IS_STUB_INTERFACE> writer_ref = writer;
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            CHECK(co_await agrpc::write(writer_ref, response));
            CHECK(co_await agrpc::finish(writer_ref, grpc::Status::OK));
        },
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            request.set_integer(42);
            test::ClientAsyncReader<IS_STUB_INTERFACE> reader;
            CHECK(co_await agrpc::request(&Stub::PrepareAsyncServerStreaming, test_stub, test.client_context, request,
                                          reader));
            CHECK(std::is_same_v<std::pair<decltype(reader), bool>,
                                 typename decltype(agrpc::request(&Stub::PrepareAsyncServerStreaming, test_stub,
                                                                  test.client_context, request))::value_type>);
            test::msg::Response response;
            CHECK(co_await agrpc::read(*reader, response));
            grpc::Status status;
            CHECK(co_await agrpc::finish(*reader, status));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
}

TEST_CASE_TEMPLATE("awaitable client streaming", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    constexpr bool IS_STUB_INTERFACE = std::is_same_v<test::v1::Test::StubInterface, Stub>;
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    test::co_spawn_and_run(
        test.grpc_context,
        [&]() -> asio::awaitable<void>
        {
            grpc::ServerAsyncReader<test::msg::Response, test::msg::Request> reader{&test.server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, test.service,
                                          test.server_context, reader));
            test::ServerAsyncReader<IS_STUB_INTERFACE> reader_ref = reader;
            test::msg::Request request;
            CHECK(co_await agrpc::read(reader_ref, request));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            CHECK(co_await agrpc::finish(reader_ref, response, grpc::Status::OK));
        },
        [&]() -> asio::awaitable<void>
        {
            test::msg::Response response;
            test::ClientAsyncWriter<IS_STUB_INTERFACE> writer;
            CHECK(co_await agrpc::request(&Stub::PrepareAsyncClientStreaming, test_stub, test.client_context, writer,
                                          response));
            CHECK(std::is_same_v<std::pair<decltype(writer), bool>,
                                 typename decltype(agrpc::request(&Stub::PrepareAsyncClientStreaming, test_stub,
                                                                  test.client_context, response))::value_type>);
            test::msg::Request request;
            request.set_integer(42);
            CHECK(co_await agrpc::write(*writer, request));
            grpc::Status status;
            CHECK(co_await agrpc::finish(*writer, status));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
}

TEST_CASE_TEMPLATE("awaitable unary", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    SUBCASE("server finish") { use_finish_with_error = false; }
    test::co_spawn_and_run(
        test.grpc_context,
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&test.server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, test.service,
                                          test.server_context, request, writer));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            if (use_finish_with_error)
            {
                CHECK(co_await agrpc::finish_with_error(writer, grpc::Status::CANCELLED));
            }
            else
            {
                CHECK(co_await agrpc::finish(writer, response, grpc::Status::OK));
            }
        },
        [&]() -> asio::awaitable<void>
        {
            using Reader = test::ClientAsyncResponseReader<std::is_same_v<test::v1::Test::StubInterface, Stub>>;
            test::msg::Request request;
            request.set_integer(42);
            auto reader = agrpc::request(&Stub::AsyncUnary, test_stub, test.client_context, request, test.grpc_context);
            test::msg::Response response;
            grpc::Status status;
            CHECK(co_await agrpc::finish(*reader, response, status));
            if (use_finish_with_error)
            {
                CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
            }
            else
            {
                CHECK(status.ok());
                CHECK_EQ(21, response.integer());
            }
        });
}

TEST_CASE_TEMPLATE("awaitable bidirectional streaming", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    constexpr bool IS_STUB_INTERFACE = std::is_same_v<test::v1::Test::StubInterface, Stub>;
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    SUBCASE("server write then finish") {}
    test::co_spawn_and_run(
        test.grpc_context,
        [&]() -> asio::awaitable<void>
        {
            grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request> reader_writer{&test.server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, test.service,
                                          test.server_context, reader_writer));
            test::ServerAsyncReaderWriter<IS_STUB_INTERFACE> reader_writer_ref = reader_writer;
            test::msg::Request request;
            CHECK(co_await agrpc::read(reader_writer_ref, request));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            if (use_write_and_finish)
            {
                CHECK(co_await agrpc::write_and_finish(reader_writer_ref, response, {}, grpc::Status::OK));
            }
            else
            {
                CHECK(co_await agrpc::write(reader_writer_ref, response));
                CHECK(co_await agrpc::finish(reader_writer_ref, grpc::Status::OK));
            }
        },
        [&]() -> asio::awaitable<void>
        {
            test::ClientAsyncReaderWriter<IS_STUB_INTERFACE> reader_writer;
            CHECK(co_await agrpc::request(&Stub::PrepareAsyncBidirectionalStreaming, test_stub, test.client_context,
                                          reader_writer));
            CHECK(std::is_same_v<std::pair<decltype(reader_writer), bool>,
                                 typename decltype(agrpc::request(&Stub::PrepareAsyncBidirectionalStreaming, test_stub,
                                                                  test.client_context))::value_type>);
            test::msg::Request request;
            request.set_integer(42);
            CHECK(co_await agrpc::write(*reader_writer, request));
            test::msg::Response response;
            CHECK(co_await agrpc::read(*reader_writer, response));
            grpc::Status status;
            CHECK(co_await agrpc::finish(*reader_writer, status));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
template <class Function>
asio::awaitable<void> run_with_deadline(grpc::Alarm& alarm, grpc::ClientContext& client_context,
                                        std::chrono::system_clock::time_point deadline, Function function)
{
    const auto set_alarm = [&]() -> asio::awaitable<void>
    {
        if (co_await agrpc::wait(alarm, deadline))
        {
            client_context.TryCancel();
        }
    };
    using namespace asio::experimental::awaitable_operators;
    co_await (set_alarm() || function());
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable run_with_deadline no cancel")
{
    bool server_finish_ok{};
    grpc::Status status;
    test::co_spawn_and_run(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request,
                                          writer));
            test::msg::Response response;
            server_finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
        },
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            const auto reader =
                agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context, request, grpc_context);
            test::msg::Response response;
            grpc::Alarm alarm;
            const auto not_too_exceed = test::one_seconds_from_now();
            co_await run_with_deadline(alarm, client_context, not_too_exceed,
                                       [&]() -> asio::awaitable<void>
                                       {
                                           CHECK(co_await agrpc::finish(*reader, response, status));
                                       });
            CHECK_LT(test::now(), not_too_exceed);
        });
    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
    CHECK(server_finish_ok);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable run_with_deadline and cancel")
{
    bool server_finish_ok{};
    grpc::Status status;
    test::co_spawn_and_run(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request,
                                          writer));
            grpc::Alarm alarm;
            co_await agrpc::wait(alarm, test::one_seconds_from_now());
            test::msg::Response response;
            server_finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
        },
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            const auto reader = stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
            test::msg::Response response;
            grpc::Alarm alarm;
            const auto not_too_exceed = test::one_seconds_from_now();
            co_await run_with_deadline(alarm, client_context, test::hundred_milliseconds_from_now(),
                                       [&]() -> asio::awaitable<void>
                                       {
                                           CHECK(co_await agrpc::finish(*reader, response, status));
                                       });
            CHECK_LT(test::now(), not_too_exceed);
        });
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK_FALSE(server_finish_ok);
}
#endif
}
#endif
