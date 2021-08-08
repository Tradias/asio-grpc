#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <doctest/doctest.h>
#include <grpcpp/alarm.h>

namespace test_asio_grpc_cpp20
{
using namespace agrpc;

TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

TEST_CASE("GrpcExecutor fulfills Executor TS concept")
{
    CHECK(asio::can_require<agrpc::GrpcExecutor, asio::execution::blocking_t::never_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcExecutor, asio::execution::blocking_t::possibly_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcExecutor, asio::execution::relationship_t::fork_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcExecutor, asio::execution::relationship_t::continuation_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcExecutor, asio::execution::outstanding_work_t::tracked_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcExecutor, asio::execution::outstanding_work_t::untracked_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcExecutor, asio::execution::allocator_t<void>>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::blocking_t::never_t>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::blocking_t::possibly_t>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::relationship_t::fork_t>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::relationship_t::continuation_t>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::outstanding_work_t::tracked_t>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::outstanding_work_t::untracked_t>::value);
    CHECK(asio::can_query<agrpc::GrpcExecutor, asio::execution::allocator_t<void>>::value);
    CHECK(asio::execution::executor<agrpc::GrpcExecutor>);
    CHECK(asio::execution::executor_of<agrpc::GrpcExecutor, asio::execution::invocable_archetype>);
    CHECK(std::is_constructible_v<asio::any_io_executor, agrpc::GrpcExecutor>);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "co_spawn two Alarms and await their ok")
{
    bool ok1 = false;
    bool ok2 = false;
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
    CHECK(std::ranges::any_of(buffer,
                              [](auto&& value)
                              {
                                  return value != std::byte{};
                              }));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "wait for Alarm with asio::awaitable<>")
{
    bool ok = false;
    test::co_spawn(grpc_context.get_executor(),
                   [&]() -> asio::awaitable<void>
                   {
                       grpc::Alarm alarm;
                       ok = co_await agrpc::wait(alarm, test::ten_milliseconds_from_now(), asio::use_awaitable);
                   });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "server streaming")
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

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "client streaming")
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

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unary")
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

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "bidirectional streaming")
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

TEST_SUITE_END();
}  // namespace test_asio_grpc_cpp20