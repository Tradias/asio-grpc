// Copyright 2021 Dennis Hezel
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

#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <boost/asio/spawn.hpp>
#include <doctest/doctest.h>
#include <grpcpp/alarm.h>

#if (BOOST_VERSION >= 107700)
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#endif

namespace test_asio_grpc_cpp20
{
using namespace agrpc;

TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

#ifdef BOOST_ASIO_HAS_CONCEPTS
TEST_CASE("GrpcExecutor fulfills Executor TS concepts")
{
    CHECK(asio::execution::executor<agrpc::GrpcExecutor>);
    CHECK(asio::execution::executor_of<agrpc::GrpcExecutor, asio::execution::invocable_archetype>);
}
#endif

#ifdef BOOST_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "co_spawn two Alarms and await their ok")
{
    bool ok1 = false;
    bool ok2 = false;
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::Awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok1 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::USE_AWAITABLE);
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::USE_AWAITABLE);
                       grpc_context.stop();
                   });
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::Awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok2 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::USE_AWAITABLE);
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(ok1);
    CHECK(ok2);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with allocator")
{
    test::co_spawn(get_pmr_executor(),
                   [&]() -> agrpc::pmr::Awaitable<void>
                   {
                       grpc::Alarm alarm;
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::pmr::USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(std::ranges::any_of(buffer,
                              [](auto&& value)
                              {
                                  return value != std::byte{};
                              }));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with asio::awaitable<>")
{
    bool ok = false;
    test::co_spawn(get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), asio::use_awaitable);
                   });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable server streaming")
{
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::v1::Request request;
                       grpc::ServerAsyncWriter<test::v1::Response> writer{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, service,
                                                     server_context, request, writer));
                       CHECK_EQ(42, request.integer());
                       test::v1::Response response;
                       response.set_integer(21);
                       CHECK(co_await agrpc::write(writer, response));
                       CHECK(co_await agrpc::finish(writer, grpc::Status::OK));
                   });
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::v1::Request request;
                       request.set_integer(42);
                       std::unique_ptr<grpc::ClientAsyncReader<test::v1::Response>> reader;
                       CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub, client_context,
                                                     request, reader));
                       CHECK(std::is_same_v<std::pair<decltype(reader), bool>,
                                            decltype(agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub,
                                                                    client_context, request))::value_type>);
                       test::v1::Response response;
                       CHECK(co_await agrpc::read(*reader, response));
                       grpc::Status status;
                       CHECK(co_await agrpc::finish(*reader, status));
                       CHECK(status.ok());
                       CHECK_EQ(21, response.integer());
                   });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable client streaming")
{
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::ServerAsyncReader<test::v1::Response, test::v1::Request> reader{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, service,
                                                     server_context, reader));
                       test::v1::Request request;
                       CHECK(co_await agrpc::read(reader, request));
                       CHECK_EQ(42, request.integer());
                       test::v1::Response response;
                       response.set_integer(21);
                       CHECK(co_await agrpc::finish(reader, response, grpc::Status::OK));
                   });
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::v1::Response response;
                       std::unique_ptr<grpc::ClientAsyncWriter<test::v1::Request>> writer;
                       CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, client_context,
                                                     writer, response));
                       CHECK(std::is_same_v<std::pair<decltype(writer), bool>,
                                            decltype(agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub,
                                                                    client_context, response))::value_type>);
                       test::v1::Request request;
                       request.set_integer(42);
                       CHECK(co_await agrpc::write(*writer, request));
                       grpc::Status status;
                       CHECK(co_await agrpc::finish(*writer, status));
                       CHECK(status.ok());
                       CHECK_EQ(21, response.integer());
                   });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable unary")
{
    bool use_finish_with_error;
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    SUBCASE("server finish with OK") { use_finish_with_error = false; }
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::v1::Request request;
                       grpc::ServerAsyncResponseWriter<test::v1::Response> writer{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service,
                                                     server_context, request, writer));
                       CHECK_EQ(42, request.integer());
                       test::v1::Response response;
                       response.set_integer(21);
                       if (use_finish_with_error)
                       {
                           CHECK(co_await agrpc::finish_with_error(writer, grpc::Status::CANCELLED));
                       }
                       else
                       {
                           CHECK(co_await agrpc::finish(writer, response, grpc::Status::OK));
                       }
                   });
    test::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            test::v1::Request request;
            request.set_integer(42);
            std::unique_ptr<grpc::ClientAsyncResponseReader<test::v1::Response>> reader;
            co_await agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context, request, reader);
            CHECK(std::is_same_v<decltype(reader), decltype(agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub,
                                                                           client_context, request))::value_type>);
            test::v1::Response response;
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
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable bidirectional streaming")
{
    bool use_write_and_finish{};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    SUBCASE("server write then finish") { use_write_and_finish = false; }
    test::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            grpc::ServerAsyncReaderWriter<test::v1::Response, test::v1::Request> reader_writer{&server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, service,
                                          server_context, reader_writer));
            test::v1::Request request;
            CHECK(co_await agrpc::read(reader_writer, request));
            CHECK_EQ(42, request.integer());
            test::v1::Response response;
            response.set_integer(21);
            if (use_write_and_finish)
            {
                CHECK(co_await agrpc::write_and_finish(reader_writer, response, {}, grpc::Status::OK));
            }
            else
            {
                CHECK(co_await agrpc::write(reader_writer, response));
                CHECK(co_await agrpc::finish(reader_writer, grpc::Status::OK));
            }
        });
    test::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            std::unique_ptr<grpc::ClientAsyncReaderWriter<test::v1::Request, test::v1::Response>> reader_writer;
            CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub, client_context,
                                          reader_writer));
            CHECK(std::is_same_v<std::pair<decltype(reader_writer), bool>,
                                 decltype(agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub,
                                                         client_context))::value_type>);
            test::v1::Request request;
            request.set_integer(42);
            CHECK(co_await agrpc::write(*reader_writer, request));
            test::v1::Response response;
            CHECK(co_await agrpc::read(*reader_writer, response));
            grpc::Status status;
            CHECK(co_await agrpc::finish(*reader_writer, status));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
    grpc_context.run();
}

#if (BOOST_VERSION >= 107700)
TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with cancellation_slot")
{
    bool ok = true;
    asio::cancellation_signal signal{};
    test::co_spawn(get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok = co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(3),
                                                 asio::bind_cancellation_slot(signal.slot(), asio::use_awaitable));
                   });
    signal.emit(asio::cancellation_type::total);
    grpc_context.run();
    CHECK_FALSE(ok);
}
#endif
#endif

TEST_SUITE_END();
}  // namespace test_asio_grpc_cpp20