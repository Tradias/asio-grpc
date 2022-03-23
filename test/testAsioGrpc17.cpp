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
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"
#include "utils/rpc.hpp"

#include <agrpc/grpcInitiate.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>
#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <thread>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0))
{
TEST_CASE("agrpc::request and agrpc::wait are noexcept for use_sender")
{
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcContext&>()));
    CHECK_FALSE(noexcept(agrpc::request(std::declval<decltype(&test::v1::Test::Stub::AsyncServerStreaming)>(),
                                        std::declval<test::v1::Test::Stub&>(), std::declval<grpc::ClientContext&>(),
                                        std::declval<test::msg::Request&>(),
                                        std::declval<std::unique_ptr<grpc::ClientAsyncReader<test::msg::Response>>&>(),
                                        std::declval<asio::yield_context>())));
    CHECK(noexcept(agrpc::request(
        std::declval<decltype(&test::v1::Test::Stub::AsyncServerStreaming)>(), std::declval<test::v1::Test::Stub&>(),
        std::declval<grpc::ClientContext&>(), std::declval<test::msg::Request&>(),
        std::declval<std::unique_ptr<grpc::ClientAsyncReader<test::msg::Response>>&>(), std::declval<UseSender&&>())));
    CHECK_FALSE(
        noexcept(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                             std::declval<asio::yield_context>())));
    CHECK(noexcept(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                               std::declval<UseSender&&>())));
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "grpc_initiate NotifyOnStateChange")
{
    bool actual_ok{false};
    bool expected_ok{true};
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    SUBCASE("success") {}
    SUBCASE("deadline expires")
    {
        actual_ok = true;
        expected_ok = false;
        deadline = std::chrono::system_clock::now() - std::chrono::seconds(5);
    }
    const auto state = channel->GetState(true);
    agrpc::grpc_initiate(
        [&](agrpc::GrpcContext& context, void* tag)
        {
            channel->NotifyOnStateChange(state, deadline, agrpc::get_completion_queue(context), tag);
        },
        asio::bind_executor(grpc_context,
                            [&](bool ok)
                            {
                                actual_ok = ok;
                            }));
    grpc_context.run();
    CHECK_EQ(expected_ok, actual_ok);
}

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/yield.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/yield.hpp>
#endif

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::coroutine with Alarm")
{
    struct Coro : asio::coroutine
    {
        using executor_type = agrpc::GrpcContext::executor_type;

        struct Context
        {
            std::chrono::system_clock::time_point deadline;
            agrpc::GrpcContext& grpc_context;
            bool& ok;
            grpc::Alarm alarm;

            Context(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context, bool& ok)
                : deadline(deadline), grpc_context(grpc_context), ok(ok)
            {
            }
        };

        std::shared_ptr<Context> context;

        Coro(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context, bool& ok)
            : context(std::make_shared<Context>(deadline, grpc_context, ok))
        {
        }

        void operator()(bool wait_ok)
        {
            reenter(*this)
            {
                yield agrpc::wait(context->alarm, context->deadline, std::move(*this));
                context->ok = wait_ok;
            }
        }

        executor_type get_executor() const noexcept { return context->grpc_context.get_executor(); }
    };
    bool ok{false};
    Coro{test::ten_milliseconds_from_now(), grpc_context, ok}(false);
    grpc_context.run();
    CHECK(ok);
}

template <class Function>
struct Coro : asio::coroutine
{
    using executor_type = std::decay_t<
        asio::require_result<agrpc::GrpcContext::executor_type, asio::execution::outstanding_work_t::tracked_t>::type>;

    executor_type executor;
    Function function;

    Coro(agrpc::GrpcContext& grpc_context, Function&& f)
        : executor(asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)),
          function(std::forward<Function>(f))
    {
    }

    void operator()(bool ok) { function(ok, *this); }

    executor_type get_executor() const noexcept { return executor; }
};

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unary stackless coroutine")
{
    grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
    test::msg::Request server_request;
    test::msg::Response server_response;
    auto server_loop = [&](bool ok, auto& coro) mutable
    {
        reenter(coro)
        {
            yield agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, server_request,
                                 writer, coro);
            CHECK(ok);
            CHECK_EQ(42, server_request.integer());
            server_response.set_integer(21);
            yield agrpc::finish(writer, server_response, grpc::Status::OK, coro);
            CHECK(ok);
        }
    };
    std::thread server_thread(
        [&, server_coro = Coro{grpc_context, std::move(server_loop)}]() mutable
        {
            server_coro(true);
        });

    test::msg::Request client_request;
    client_request.set_integer(42);
    test::msg::Response client_response;
    grpc::Status status;
    std::unique_ptr<grpc::ClientAsyncResponseReader<test::msg::Response>> reader;
    auto client_loop = [&](bool ok, auto& coro) mutable
    {
        reenter(coro)
        {
            reader = stub->AsyncUnary(&client_context, client_request, agrpc::get_completion_queue(coro));
            yield agrpc::finish(*reader, client_response, status, coro);
            CHECK(ok);
            CHECK(status.ok());
            CHECK_EQ(21, client_response.integer());
        }
    };
    std::thread client_thread(
        [&, client_coro = Coro{grpc_context, std::move(client_loop)}]() mutable
        {
            client_coro(true);
        });

    grpc_context.run();
    server_thread.join();
    client_thread.join();
}

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/unyield.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/unyield.hpp>
#endif

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context server streaming")
{
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    bool use_write_last{false};
    SUBCASE("server write_last") { use_write_last = true; }
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::msg::Request request;
                    grpc::ServerAsyncWriter<test::msg::Response> writer{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, service, server_context,
                                         request, writer, yield));
                    CHECK(agrpc::send_initial_metadata(writer, yield));
                    CHECK_EQ(42, request.integer());
                    test::msg::Response response;
                    response.set_integer(21);
                    CHECK(agrpc::write(writer, response, grpc::WriteOptions{}, yield));
                    if (use_write_and_finish)
                    {
                        CHECK(agrpc::write_and_finish(writer, response, {}, grpc::Status::OK, yield));
                    }
                    else
                    {
                        if (use_write_last)
                        {
                            CHECK(agrpc::write_last(writer, response, grpc::WriteOptions{}, yield));
                        }
                        else
                        {
                            CHECK(agrpc::write(writer, response, yield));
                        }
                        CHECK(agrpc::finish(writer, grpc::Status::OK, yield));
                    }
                });
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::msg::Request request;
                    request.set_integer(42);
                    auto [reader, ok] = [&]
                    {
                        if (use_client_convenience)
                        {
                            return agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub, client_context,
                                                  request, yield);
                        }
                        std::unique_ptr<grpc::ClientAsyncReader<test::msg::Response>> reader;
                        bool ok = agrpc::request(&test::v1::Test::Stub::AsyncServerStreaming, *stub, client_context,
                                                 request, reader, yield);
                        return std::pair{std::move(reader), ok};
                    }();
                    CHECK(ok);
                    CHECK(agrpc::read_initial_metadata(*reader, yield));
                    test::msg::Response response;
                    CHECK(agrpc::read(*reader, response, yield));
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
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    bool use_write_last{false};
    SUBCASE("client write_last") { use_write_last = true; }
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    grpc::ServerAsyncReader<test::msg::Response, test::msg::Request> reader{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, service, server_context,
                                         reader, yield));
                    CHECK(agrpc::send_initial_metadata(reader, yield));
                    test::msg::Request request;
                    CHECK(agrpc::read(reader, request, yield));
                    CHECK_EQ(42, request.integer());
                    CHECK(agrpc::read(reader, request, yield));
                    CHECK_EQ(42, request.integer());
                    CHECK_FALSE(agrpc::read(reader, request, yield));
                    test::msg::Response response;
                    response.set_integer(21);
                    if (use_finish_with_error)
                    {
                        CHECK(agrpc::finish_with_error(reader, grpc::Status::CANCELLED, yield));
                    }
                    else
                    {
                        CHECK(agrpc::finish(reader, response, grpc::Status::OK, yield));
                    }
                });
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::msg::Response response;
                    auto [writer, ok] = [&]
                    {
                        if (use_client_convenience)
                        {
                            return agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, client_context,
                                                  response, yield);
                        }
                        std::unique_ptr<grpc::ClientAsyncWriter<test::msg::Request>> writer;
                        bool ok = agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, client_context,
                                                 writer, response, yield);
                        return std::pair{std::move(writer), ok};
                    }();
                    CHECK(ok);
                    test::client_perform_client_streaming_success(response, *writer, yield,
                                                                  {use_finish_with_error, use_write_last});
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context unary")
{
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    SUBCASE("server finish with OK") {}
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::msg::Request request;
                    grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
                    CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request,
                                         writer, yield));
                    CHECK(agrpc::send_initial_metadata(writer, yield));
                    CHECK_EQ(42, request.integer());
                    test::msg::Response response;
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
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    test::client_perform_unary_success(grpc_context, *stub, yield, {use_finish_with_error});
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "yield_context bidirectional streaming")
{
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    bool use_write_last{false};
    SUBCASE("write_last") { use_write_last = true; }
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    bool set_initial_metadata_corked{false};
    SUBCASE("client set initial metadata corked") { set_initial_metadata_corked = true; }
    asio::spawn(
        get_executor(),
        [&](asio::yield_context yield)
        {
            grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request> reader_writer{&server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, service, server_context,
                                 reader_writer, yield));
            CHECK(agrpc::send_initial_metadata(reader_writer, yield));
            test::msg::Request request;
            CHECK(agrpc::read(reader_writer, request, yield));
            CHECK(agrpc::read(reader_writer, request, yield));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            CHECK(agrpc::write(reader_writer, response, grpc::WriteOptions{}, yield));
            if (use_write_and_finish)
            {
                CHECK(agrpc::write_and_finish(reader_writer, response, {}, grpc::Status::OK, yield));
            }
            else
            {
                if (use_write_last)
                {
                    CHECK(agrpc::write_last(reader_writer, response, {}, yield));
                }
                else
                {
                    CHECK(agrpc::write(reader_writer, response, yield));
                }
                CHECK(agrpc::finish(reader_writer, grpc::Status::OK, yield));
            }
        });
    asio::spawn(
        get_executor(),
        [&](asio::yield_context yield)
        {
            auto [reader_writer, ok] = [&]
            {
                if (use_client_convenience)
                {
                    return agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub, client_context,
                                          yield);
                }
                else if (set_initial_metadata_corked)
                {
                    client_context.set_initial_metadata_corked(true);
                    return std::pair{stub->AsyncBidirectionalStreaming(
                                         &client_context, agrpc::get_completion_queue(grpc_context), nullptr),
                                     true};
                }
                std::unique_ptr<grpc::ClientAsyncReaderWriter<test::msg::Request, test::msg::Response>> reader_writer;
                bool ok = agrpc::request(&test::v1::Test::Stub::AsyncBidirectionalStreaming, *stub, client_context,
                                         reader_writer, yield);
                return std::pair{std::move(reader_writer), ok};
            }();
            if (!set_initial_metadata_corked)
            {
                CHECK(ok);
                CHECK(agrpc::read_initial_metadata(*reader_writer, yield));
            }
            test::msg::Request request;
            request.set_integer(42);
            CHECK(agrpc::write(*reader_writer, request, yield));
            if (use_write_last)
            {
                CHECK(agrpc::write_last(*reader_writer, request, grpc::WriteOptions{}, yield));
            }
            else
            {
                CHECK(agrpc::write(*reader_writer, request, grpc::WriteOptions{}, yield));
                CHECK(agrpc::writes_done(*reader_writer, yield));
            }
            test::msg::Response response;
            CHECK(agrpc::read(*reader_writer, response, yield));
            CHECK(agrpc::read(*reader_writer, response, yield));
            grpc::Status status;
            CHECK(agrpc::finish(*reader_writer, status, yield));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "RPC step after grpc_context stop")
{
    std::optional<bool> ok;
    asio::spawn(get_executor(),
                [&](asio::yield_context yield)
                {
                    grpc_context.stop();
                    test::msg::Request request;
                    grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
                    ok = agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context, request,
                                        writer, yield);
                });
    grpc_context.run();
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::post an Alarm and use variadic-arg callback for its wait")
{
    bool ok{false};
    grpc::Alarm alarm;
    asio::post(get_executor(),
               [&]
               {
                   agrpc::wait(alarm, test::ten_milliseconds_from_now(),
                               asio::bind_executor(get_executor(),
                                                   [&](auto&&... args)
                                                   {
                                                       ok = bool{args...};
                                                   }));
               });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::wait with const-ref callback")
{
    grpc::Alarm alarm;
    bool ok{false};
    const auto cb = asio::bind_executor(grpc_context,
                                        [&](bool wait_ok)
                                        {
                                            ok = wait_ok;
                                        });
    agrpc::wait(alarm, test::ten_milliseconds_from_now(), cb);
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::wait with move-only callback")
{
    struct Cb
    {
        using executor_type = agrpc::GrpcExecutor;

        executor_type executor;
        std::unique_ptr<int>& target;
        std::unique_ptr<int> ptr{std::make_unique<int>(42)};

        void operator()(bool) { target = std::move(ptr); }

        auto get_executor() const { return executor; }
    };
    grpc::Alarm alarm;
    std::unique_ptr<int> ptr;
    agrpc::wait(alarm, test::ten_milliseconds_from_now(), Cb{get_executor(), ptr});
    grpc_context.run();
    REQUIRE(ptr);
    CHECK_EQ(42, *ptr);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with cancellation_type::total")
{
    bool ok{true};
    asio::cancellation_signal signal{};
    grpc::Alarm alarm;
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    agrpc::wait(alarm, test::five_seconds_from_now(),
                asio::bind_cancellation_slot(signal.slot(), asio::bind_executor(get_executor(),
                                                                                [&](bool alarm_ok)
                                                                                {
                                                                                    ok = alarm_ok;
                                                                                })));
    asio::post(get_executor(),
               [&]
               {
                   signal.emit(asio::cancellation_type::total);
               });
    SUBCASE("cancel once") {}
    SUBCASE("cancel twice has no effect")
    {
        asio::post(get_executor(),
                   [&]
                   {
                       signal.emit(asio::cancellation_type::total);
                   });
    }
    grpc_context.run();
    CHECK_GT(not_too_exceed, std::chrono::steady_clock::now());
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with cancellation_type::none")
{
    bool ok{false};
    asio::cancellation_signal signal{};
    grpc::Alarm alarm;
    asio::post(get_executor(),
               [&]
               {
                   agrpc::wait(alarm, test::hundred_milliseconds_from_now(),
                               asio::bind_cancellation_slot(signal.slot(), asio::bind_executor(get_executor(),
                                                                                               [&](bool alarm_ok)
                                                                                               {
                                                                                                   ok = alarm_ok;
                                                                                               })));
                   asio::post(get_executor(),
                              [&]
                              {
                                  signal.emit(asio::cancellation_type::none);
                              });
               });
    grpc_context.run();
    CHECK(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with parallel_group")
{
    std::array<std::size_t, 2> completion_order;
    std::optional<test::ErrorCode> error_code;
    bool ok{true};
    grpc::Alarm alarm;
    asio::steady_timer timer{get_executor(), std::chrono::milliseconds(100)};
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    asio::experimental::make_parallel_group(timer.async_wait(asio::experimental::deferred),
                                            [&](auto token)
                                            {
                                                return agrpc::wait(alarm, test::five_seconds_from_now(),
                                                                   asio::bind_executor(get_executor(), token));
                                            })
        .async_wait(asio::experimental::wait_for_one(),
                    [&](std::array<std::size_t, 2> actual_completion_order, test::ErrorCode timer_ec, bool wait_ok)
                    {
                        completion_order = actual_completion_order;
                        error_code.emplace(timer_ec);
                        ok = wait_ok;
                    });
    grpc_context.run();
    CHECK_GT(not_too_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, completion_order[0]);
    CHECK_EQ(1, completion_order[1]);
    CHECK_EQ(test::ErrorCode{}, error_code);
    CHECK_FALSE(ok);
}
#endif
}