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

#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <doctest/doctest.h>

#include <cstddef>

namespace test_asio_grpc_cpp20
{
TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

#ifdef AGRPC_ASIO_HAS_CONCEPTS
TEST_CASE("GrpcExecutor fulfills Executor TS concepts")
{
    CHECK(asio::execution::executor<agrpc::GrpcExecutor>);
    CHECK(asio::execution::executor_of<agrpc::GrpcExecutor, test::InvocableArchetype>);
}

TEST_CASE("asio-grpc fulfills unified executor concepts")
{
    CHECK(asio::execution::is_scheduler_v<agrpc::GrpcExecutor>);
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcExecutor>()));
    using UseSenderFromGrpcContext = decltype(agrpc::use_sender(std::declval<agrpc::GrpcContext&>()));
    CHECK(std::is_same_v<UseSender, UseSenderFromGrpcContext>);
    using GrpcSender =
        decltype(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                             std::declval<UseSender>()));
    CHECK(asio::execution::sender<GrpcSender>);
    CHECK(asio::execution::is_sender_v<GrpcSender>);
    CHECK(asio::execution::typed_sender<GrpcSender>);
    CHECK(asio::execution::is_typed_sender_v<GrpcSender>);
    CHECK(asio::execution::sender_to<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_sender_to_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_nothrow_connect_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    using OperationState = asio::execution::connect_result_t<GrpcSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<OperationState>);
    CHECK(asio::execution::is_operation_state_v<OperationState>);

    using ScheduleSender = decltype(asio::execution::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(asio::execution::sender<ScheduleSender>);
    CHECK(asio::execution::is_sender_v<ScheduleSender>);
    CHECK(asio::execution::typed_sender<ScheduleSender>);
    CHECK(asio::execution::is_typed_sender_v<ScheduleSender>);
    CHECK(asio::execution::sender_to<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_sender_to_v<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_nothrow_connect_v<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    using ScheduleSenderOperationState = asio::execution::connect_result_t<ScheduleSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<ScheduleSenderOperationState>);
    CHECK(asio::execution::is_operation_state_v<ScheduleSenderOperationState>);
}
#endif

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio GrpcExecutor::schedule")
{
    bool is_invoked{false};
    auto sender = asio::execution::schedule(get_executor());
    test::FunctionAsReceiver receiver{[&]
                                      {
                                          is_invoked = true;
                                      }};
    auto operation_state = asio::execution::connect(std::move(sender), std::move(receiver));
    asio::execution::start(operation_state);
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
    CHECK_FALSE(receiver.was_done);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio GrpcExecutor::submit with allocator")
{
    asio::execution::submit(asio::execution::schedule(get_executor()),
                            test::FunctionAsReceiver{[] {}, get_allocator()});
    grpc_context.run();
    CHECK(std::any_of(buffer.begin(), buffer.end(),
                      [](auto&& value)
                      {
                          return value != std::byte{};
                      }));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::execution connect and start Alarm")
{
    bool ok{false};
    grpc::Alarm alarm;
    auto wait_sender = agrpc::wait(alarm, test::ten_milliseconds_from_now(), use_sender());
    test::FunctionAsReceiver receiver{[&](bool wait_ok)
                                      {
                                          ok = wait_ok;
                                      }};
    auto operation_state = asio::execution::connect(std::move(wait_sender), std::move(receiver));
    asio::execution::start(operation_state);
    grpc_context.run();
    CHECK(ok);
}

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
TEST_CASE_FIXTURE(test::GrpcContextTest, "get_completion_queue")
{
    grpc::CompletionQueue* queue{};
    SUBCASE("GrpcAwaitable")
    {
        test::co_spawn(grpc_context,
                       [&]() -> agrpc::GrpcAwaitable<void>
                       {
                           queue = co_await agrpc::get_completion_queue(agrpc::GRPC_USE_AWAITABLE);
                       });
    }
    SUBCASE("asio::awaitable")
    {
        test::co_spawn(grpc_context,
                       [&]() -> asio::awaitable<void>
                       {
                           queue = co_await agrpc::get_completion_queue();
                       });
    }
    grpc_context.run();
    CHECK_EQ(grpc_context.get_completion_queue(), queue);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "co_spawn two Alarms and await their ok")
{
    bool ok1{false};
    bool ok2{false};
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok1 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       grpc_context.stop();
                   });
    test::co_spawn(grpc_context,
                   [&]() -> agrpc::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok2 = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(ok1);
    CHECK(ok2);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with allocator")
{
    test::co_spawn(get_pmr_executor(),
                   [&]() -> agrpc::pmr::GrpcAwaitable<void>
                   {
                       grpc::Alarm alarm;
                       co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::pmr::GRPC_USE_AWAITABLE);
                   });
    grpc_context.run();
    CHECK(std::any_of(buffer.begin(), buffer.end(),
                      [](auto&& value)
                      {
                          return value != std::byte{};
                      }));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with asio::awaitable<>")
{
    bool ok{false};
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
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
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
            auto reader =
                co_await [&]() -> asio::awaitable<std::unique_ptr<grpc::ClientAsyncResponseReader<test::v1::Response>>>
            {
                if (use_client_convenience)
                {
                    co_return co_await agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context,
                                                      request);
                }
                std::unique_ptr<grpc::ClientAsyncResponseReader<test::v1::Response>> reader;
                co_await agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context, request, reader);
                co_return reader;
            }();
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
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    SUBCASE("server write then finish") {}
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

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "repeatedly_request with asio use_sender")
{
    bool is_shutdown{false};
    auto request_count{0};
    test::v1::Response response;
    asio::execution::submit(agrpc::repeatedly_request(
                                &test::v1::Test::AsyncService::RequestUnary, service,
                                [&](grpc::ServerContext&, test::v1::Request& request,
                                    grpc::ServerAsyncResponseWriter<test::v1::Response>& writer)
                                {
                                    CHECK_EQ(42, request.integer());
                                    ++request_count;
                                    if (request_count > 3)
                                    {
                                        is_shutdown = true;
                                    }
                                    response.set_integer(21);
                                    return agrpc::finish(writer, response, grpc::Status::OK, use_sender());
                                },
                                use_sender()),
                            test::FunctionAsReceiver{[&]()
                                                     {
                                                         CHECK_EQ(4, request_count);
                                                     }});
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       while (!is_shutdown)
                       {
                           grpc::ClientContext new_client_context;
                           test::v1::Request request;
                           request.set_integer(42);
                           const auto reader =
                               stub->AsyncUnary(&new_client_context, request, grpc_context.get_completion_queue());
                           test::v1::Response response;
                           grpc::Status status;
                           CHECK(co_await agrpc::finish(*reader, response, status));
                           CHECK(status.ok());
                           CHECK_EQ(21, response.integer());
                       }
                       server->Shutdown();
                   });
    grpc_context.run();
    CHECK_EQ(4, request_count);
}
#endif

TEST_SUITE_END();
}  // namespace test_asio_grpc_cpp20