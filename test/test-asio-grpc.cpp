#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <doctest/doctest.h>
#include <grpcpp/alarm.h>

#include <memory_resource>
#include <optional>
#include <string_view>
#include <thread>

namespace test_asio_grpc
{
using namespace agrpc;

TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

TEST_CASE("GrpcExecutor fulfills Executor TS traits")
{
    CHECK(asio::execution::can_execute_v<std::add_const_t<agrpc::GrpcContext::executor_type>,
                                         asio::execution::invocable_archetype>);
    CHECK(asio::execution::is_executor_v<agrpc::GrpcContext::executor_type>);
    CHECK(asio::can_require<agrpc::GrpcContext::executor_type, asio::execution::blocking_t::never_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcContext::executor_type, asio::execution::blocking_t::possibly_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcContext::executor_type, asio::execution::relationship_t::fork_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcContext::executor_type, asio::execution::relationship_t::continuation_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcContext::executor_type, asio::execution::outstanding_work_t::tracked_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcContext::executor_type, asio::execution::outstanding_work_t::untracked_t>::value);
    CHECK(asio::can_prefer<agrpc::GrpcContext::executor_type,
                           asio::execution::allocator_t<std::pmr::polymorphic_allocator<std::byte>>>::value);
    CHECK(asio::can_query<agrpc::GrpcContext::executor_type, asio::execution::blocking_t>::value);
    CHECK(asio::can_query<agrpc::GrpcContext::executor_type, asio::execution::relationship_t>::value);
    CHECK(asio::can_query<agrpc::GrpcContext::executor_type, asio::execution::outstanding_work_t>::value);
    CHECK(asio::can_query<agrpc::GrpcContext::executor_type, asio::execution::mapping_t>::value);
    CHECK(asio::can_query<agrpc::GrpcContext::executor_type, asio::execution::allocator_t<void>>::value);
    CHECK(asio::can_query<agrpc::GrpcContext::executor_type, asio::execution::context_t>::value);
    CHECK(std::is_constructible_v<asio::any_io_executor, agrpc::GrpcContext::executor_type>);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcExecutor is mostly trivial")
{
    CHECK(std::is_trivially_copy_constructible_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_move_constructible_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_destructible_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_copy_assignable_v<agrpc::GrpcExecutor>);
    CHECK(std::is_trivially_move_assignable_v<agrpc::GrpcExecutor>);
    CHECK_EQ(sizeof(void*), sizeof(agrpc::GrpcExecutor));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::spawn an Alarm and yield its wait")
{
    bool ok = false;
    asio::spawn(asio::bind_executor(get_work_tracking_executor(), [] {}),
                [&](auto&& yield)
                {
                    grpc::Alarm alarm;
                    ok = agrpc::wait(alarm, test::ten_milliseconds_from_now(), yield);
                });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::spawn with yield_context")
{
    bool ok = false;
    std::optional<asio::executor_work_guard<agrpc::GrpcExecutor>> guard;
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    grpc::Alarm alarm;
                    ok = agrpc::wait(alarm, test::ten_milliseconds_from_now(), yield);
                    guard.reset();
                });
    guard.emplace(asio::make_work_guard(grpc_context));
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "post from multiple threads")
{
    static constexpr auto THREAD_COUNT = 32;
    std::atomic_int counter{};
    asio::thread_pool pool{THREAD_COUNT};
    auto guard = asio::make_work_guard(grpc_context);
    for (size_t i = 0; i < THREAD_COUNT; ++i)
    {
        asio::post(pool,
                   [&]
                   {
                       asio::post(grpc_context,
                                  [&]
                                  {
                                      if (++counter == THREAD_COUNT)
                                      {
                                          guard.reset();
                                      }
                                  });
                   });
    }
    asio::post(pool,
               [&]
               {
                   grpc_context.run();
               });
    pool.join();
    CHECK_EQ(THREAD_COUNT, counter);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "post/execute with allocator")
{
    SUBCASE("asio::post")
    {
        asio::post(grpc_context,
                   test::HandlerWithAssociatedAllocator{[] {}, std::pmr::polymorphic_allocator<std::byte>(&resource)});
    }
    SUBCASE("asio::execute")
    {
        get_pmr_executor().execute([] {});
    }
    SUBCASE("agrpc::wait")
    {
        asio::execution::execute(
            get_executor(),
            [&, executor = asio::require(get_pmr_executor(), asio::execution::outstanding_work.tracked)]() mutable
            {
                auto alarm = std::make_shared<grpc::Alarm>();
                auto& alarm_ref = *alarm;
                agrpc::wait(alarm_ref, test::ten_milliseconds_from_now(),
                            asio::bind_executor(std::move(executor),
                                                [a = std::move(alarm)](bool ok)
                                                {
                                                    CHECK(ok);
                                                }));
            });
    }
    grpc_context.run();
    CHECK(std::any_of(buffer.begin(), buffer.end(),
                      [](auto&& value)
                      {
                          return value != std::byte{};
                      }));
}

template <class Function>
struct Coro : asio::coroutine
{
    using executor_type =
        asio::require_result<agrpc::GrpcContext::executor_type, asio::execution::outstanding_work_t::tracked_t>::type;

    executor_type executor;
    Function function;

    Coro(agrpc::GrpcContext& grpc_context, Function&& f)
        : executor(asio::require(grpc_context.get_executor(), asio::execution::outstanding_work.tracked)),
          function(std::forward<Function>(f))
    {
    }

    void operator()(bool ok) { function(ok, this); }

    executor_type get_executor() const noexcept { return executor; }
};

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unary stackless coroutine")
{
    grpc::ServerAsyncResponseWriter<test::v1::Response> writer{&server_context};
    test::v1::Request server_request;
    test::v1::Response server_response;
    auto server_loop = [&](bool ok, auto* coro) mutable
    {
        BOOST_ASIO_CORO_REENTER(*coro)
        {
            BOOST_ASIO_CORO_YIELD
            agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, server_request, writer,
                           *coro);
            CHECK(ok);
            CHECK_EQ(42, server_request.integer());
            server_response.set_integer(21);
            BOOST_ASIO_CORO_YIELD agrpc::finish(writer, server_response, grpc::Status::OK, *coro);
            CHECK(ok);
        }
    };
    asio::post(grpc_context,
               [&, server_coro = Coro{grpc_context, std::move(server_loop)}]() mutable
               {
                   server_coro(true);
               });

    test::v1::Request client_request;
    client_request.set_integer(42);
    test::v1::Response client_response;
    grpc::Status status;
    std::unique_ptr<grpc::ClientAsyncResponseReader<test::v1::Response>> reader;
    auto client_loop = [&](bool ok, auto* coro) mutable
    {
        BOOST_ASIO_CORO_REENTER(*coro)
        {
            reader = stub->AsyncUnary(&client_context, client_request, agrpc::get_completion_queue(*coro));
            BOOST_ASIO_CORO_YIELD agrpc::finish(*reader, client_response, status, *coro);
            CHECK(ok);
            CHECK(status.ok());
            CHECK_EQ(21, client_response.integer());
        }
    };
    asio::post(grpc_context,
               [&, client_coro = Coro{grpc_context, std::move(client_loop)}]() mutable
               {
                   client_coro(true);
               });

    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context server streaming")
{
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Request request;
                    grpc::ServerAsyncWriter<test::v1::Response> writer{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, service, server_context,
                                         request, writer, yield));
                    agrpc::send_initial_metadata(writer, yield);
                    CHECK_EQ(42, request.integer());
                    test::v1::Response response;
                    response.set_integer(21);
                    CHECK(agrpc::write(writer, response, yield));
                    CHECK(agrpc::finish(writer, grpc::Status::OK, yield));
                });
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Request request;
                    request.set_integer(42);
                    std::unique_ptr<grpc::ClientAsyncReader<test::v1::Response>> reader;
                    CHECK(agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub, client_context, request,
                                         reader, yield));
                    CHECK(std::is_same_v<std::pair<decltype(reader), bool>,
                                         decltype(agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub,
                                                                 client_context, request, yield))>);
                    agrpc::read_initial_metadata(*reader, yield);
                    test::v1::Response response;
                    CHECK(agrpc::read(*reader, response, yield));
                    grpc::Status status;
                    CHECK(agrpc::finish(*reader, status, yield));
                    CHECK(status.ok());
                    CHECK_EQ(21, response.integer());
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context client streaming")
{
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    grpc::ServerAsyncReader<test::v1::Response, test::v1::Request> reader{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, service, server_context,
                                         reader, yield));
                    agrpc::send_initial_metadata(reader, yield);
                    test::v1::Request request;
                    CHECK(agrpc::read(reader, request, yield));
                    CHECK_EQ(42, request.integer());
                    test::v1::Response response;
                    response.set_integer(21);
                    CHECK(agrpc::finish(reader, response, grpc::Status::OK, yield));
                });
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Response response;
                    std::unique_ptr<grpc::ClientAsyncWriter<test::v1::Request>> writer;
                    CHECK(agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, client_context, writer,
                                         response, yield));
                    CHECK(std::is_same_v<std::pair<decltype(writer), bool>,
                                         decltype(agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub,
                                                                 client_context, response, yield))>);
                    agrpc::read_initial_metadata(*writer, yield);
                    test::v1::Request request;
                    request.set_integer(42);
                    CHECK(agrpc::write(*writer, request, yield));
                    CHECK(agrpc::writes_done(*writer, yield));
                    grpc::Status status;
                    CHECK(agrpc::finish(*writer, status, yield));
                    CHECK(status.ok());
                    CHECK_EQ(21, response.integer());
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context unary")
{
    bool use_finish_with_error;
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    SUBCASE("server finish with OK") { use_finish_with_error = false; }
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Request request;
                    grpc::ServerAsyncResponseWriter<test::v1::Response> writer{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request,
                                         writer, yield));
                    agrpc::send_initial_metadata(writer, yield);
                    CHECK_EQ(42, request.integer());
                    test::v1::Response response;
                    response.set_integer(21);
                    if (use_finish_with_error)
                    {
                        CHECK(agrpc::finish_with_error(writer, grpc::Status::CANCELLED, yield));
                    }
                    else
                    {
                        CHECK(agrpc::finish(writer, response, grpc::Status::OK, yield));
                    }
                });
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    test::v1::Request request;
                    request.set_integer(42);
                    auto reader =
                        stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(get_executor()));
                    agrpc::read_initial_metadata(*reader, yield);
                    test::v1::Response response;
                    grpc::Status status;
                    CHECK(agrpc::finish(*reader, response, status, yield));
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

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context bidirectional streaming")
{
    bool use_write_and_finish{};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    SUBCASE("server write then finish") { use_write_and_finish = false; }
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    grpc::ServerAsyncReaderWriter<test::v1::Response, test::v1::Request> reader_writer{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, service,
                                         server_context, reader_writer, yield));
                    agrpc::send_initial_metadata(reader_writer, yield);
                    test::v1::Request request;
                    CHECK(agrpc::read(reader_writer, request, yield));
                    CHECK_EQ(42, request.integer());
                    test::v1::Response response;
                    response.set_integer(21);
                    if (use_write_and_finish)
                    {
                        CHECK(agrpc::write_and_finish(reader_writer, response, {}, grpc::Status::OK, yield));
                    }
                    else
                    {
                        CHECK(agrpc::write(reader_writer, response, yield));
                        CHECK(agrpc::finish(reader_writer, grpc::Status::OK, yield));
                    }
                });
    asio::spawn(get_work_tracking_executor(),
                [&](asio::yield_context yield)
                {
                    std::unique_ptr<grpc::ClientAsyncReaderWriter<test::v1::Request, test::v1::Response>> reader_writer;
                    CHECK(agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub, client_context,
                                         reader_writer, yield));
                    CHECK(std::is_same_v<std::pair<decltype(reader_writer), bool>,
                                         decltype(agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming,
                                                                 *stub, client_context, yield))>);
                    agrpc::read_initial_metadata(*reader_writer, yield);
                    test::v1::Request request;
                    request.set_integer(42);
                    CHECK(agrpc::write(*reader_writer, request, yield));
                    CHECK(agrpc::writes_done(*reader_writer, yield));
                    test::v1::Response response;
                    CHECK(agrpc::read(*reader_writer, response, yield));
                    grpc::Status status;
                    CHECK(agrpc::finish(*reader_writer, status, yield));
                    CHECK(status.ok());
                    CHECK_EQ(21, response.integer());
                });
    grpc_context.run();
}

TEST_SUITE_END();
}  // namespace test_asio_grpc