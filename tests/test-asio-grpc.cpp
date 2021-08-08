#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <boost/asio/spawn.hpp>
#include <doctest/doctest.h>
#include <grpcpp/alarm.h>
#include <grpcpp/grpcpp.h>

#include <memory_resource>
#include <string_view>
#include <thread>

namespace test_asio_grpc
{
using namespace agrpc;

TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

TEST_CASE_FIXTURE(test::GrpcContextTest, "GrpcExecutor fulfills Boost.Asio executor concept")
{
    CHECK(asio::is_executor<agrpc::GrpcExecutor>::value);
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
    asio::spawn(asio::bind_executor(get_pmr_executor().require(asio::execution::outstanding_work.tracked), [] {}),
                [&](auto&& yield)
                {
                    grpc::Alarm alarm;
                    ok = agrpc::wait(alarm, test::ten_milliseconds_from_now(), yield);
                });
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
        grpc_context.get_executor().execute(
            [&, executor = get_pmr_executor().require(asio::execution::outstanding_work.tracked)]
            {
                grpc::Alarm alarm;
                agrpc::wait(alarm, test::ten_milliseconds_from_now(), asio::bind_executor(executor, [] {}));
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
        decltype(std::declval<agrpc::GrpcContext::executor_type&>().require(asio::execution::outstanding_work.tracked));

    executor_type executor;
    Function function;

    Coro(agrpc::GrpcContext& grpc_context, Function&& f)
        : executor(grpc_context.get_executor().require(asio::execution::outstanding_work.tracked)),
          function(std::forward<Function>(f))
    {
    }

    void operator()(bool ok) { function(ok, this); }

    executor_type get_executor() const noexcept { return executor; }
};

TEST_CASE_FIXTURE(test::GrpcContextClientServerTest, "unary stackless coroutine")
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

TEST_SUITE_END();
}  // namespace test_asio_grpc