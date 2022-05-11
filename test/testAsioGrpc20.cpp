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
#include "utils/time.hpp"

#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>

#include <cstddef>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable server streaming")
{
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       grpc::ServerAsyncWriter<test::msg::Response> writer{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, service,
                                                     server_context, request, writer));
                       CHECK_EQ(42, request.integer());
                       test::msg::Response response;
                       response.set_integer(21);
                       CHECK(co_await agrpc::write(writer, response));
                       CHECK(co_await agrpc::finish(writer, grpc::Status::OK));
                   });
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       request.set_integer(42);
                       std::unique_ptr<grpc::ClientAsyncReader<test::msg::Response>> reader;
                       CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub, client_context,
                                                     request, reader));
                       CHECK(std::is_same_v<std::pair<decltype(reader), bool>,
                                            decltype(agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub,
                                                                    client_context, request))::value_type>);
                       test::msg::Response response;
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
                       grpc::ServerAsyncReader<test::msg::Response, test::msg::Request> reader{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, service,
                                                     server_context, reader));
                       test::msg::Request request;
                       CHECK(co_await agrpc::read(reader, request));
                       CHECK_EQ(42, request.integer());
                       test::msg::Response response;
                       response.set_integer(21);
                       CHECK(co_await agrpc::finish(reader, response, grpc::Status::OK));
                   });
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Response response;
                       std::unique_ptr<grpc::ClientAsyncWriter<test::msg::Request>> writer;
                       CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, client_context,
                                                     writer, response));
                       CHECK(std::is_same_v<std::pair<decltype(writer), bool>,
                                            decltype(agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub,
                                                                    client_context, response))::value_type>);
                       test::msg::Request request;
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
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service,
                                                     server_context, request, writer));
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
                   });
    test::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            test::msg::Request request;
            request.set_integer(42);
            auto reader =
                co_await [&]() -> asio::awaitable<std::unique_ptr<grpc::ClientAsyncResponseReader<test::msg::Response>>>
            {
                if (use_client_convenience)
                {
                    co_return co_await agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context,
                                                      request);
                }
                std::unique_ptr<grpc::ClientAsyncResponseReader<test::msg::Response>> reader;
                co_await agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context, request, reader);
                co_return reader;
            }();
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
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable bidirectional streaming")
{
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    SUBCASE("server write then finish") {}
    test::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request> reader_writer{&server_context};
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, service,
                                          server_context, reader_writer));
            test::msg::Request request;
            CHECK(co_await agrpc::read(reader_writer, request));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
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
            std::unique_ptr<grpc::ClientAsyncReaderWriter<test::msg::Request, test::msg::Response>> reader_writer;
            CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub, client_context,
                                          reader_writer));
            CHECK(std::is_same_v<std::pair<decltype(reader_writer), bool>,
                                 decltype(agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub,
                                                         client_context))::value_type>);
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
    grpc_context.run();
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
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service,
                                                     server_context, request, writer));
                       test::msg::Response response;
                       server_finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
                   });
    grpc::Status status;
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       const auto reader = agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context,
                                                          request, grpc_context);
                       test::msg::Response response;
                       grpc::Alarm alarm;
                       const auto not_too_exceed = test::one_seconds_from_now();
                       co_await run_with_deadline(alarm, client_context, not_too_exceed,
                                                  [&]() -> asio::awaitable<void>
                                                  {
                                                      CHECK(co_await agrpc::finish(*reader, response, status));
                                                  });
                       CHECK_LT(std::chrono::system_clock::now(), not_too_exceed);
                   });
    grpc_context.run();
    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
    CHECK(server_finish_ok);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable run_with_deadline and cancel")
{
    bool server_finish_ok{};
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
                       CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service,
                                                     server_context, request, writer));
                       grpc::Alarm alarm;
                       co_await agrpc::wait(alarm, test::one_seconds_from_now());
                       test::msg::Response response;
                       server_finish_ok = co_await agrpc::finish(writer, response, grpc::Status::OK);
                   });
    grpc::Status status;
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       const auto reader =
                           stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
                       test::msg::Response response;
                       grpc::Alarm alarm;
                       const auto not_too_exceed = test::one_seconds_from_now();
                       co_await run_with_deadline(alarm, client_context, test::hundred_milliseconds_from_now(),
                                                  [&]() -> asio::awaitable<void>
                                                  {
                                                      CHECK(co_await agrpc::finish(*reader, response, status));
                                                  });
                       CHECK_LT(std::chrono::system_clock::now(), not_too_exceed);
                   });
    grpc_context.run();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK_FALSE(server_finish_ok);
}
#endif
}
#endif
