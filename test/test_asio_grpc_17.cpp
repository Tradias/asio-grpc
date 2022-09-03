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
#include "utils/counting_allocator.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/rpc.hpp"

#include <agrpc/get_completion_queue.hpp>
#include <agrpc/grpc_initiate.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>
#include <grpcpp/create_channel.h>

#include <cstddef>
#include <optional>
#include <thread>

TYPE_TO_STRING(test::v1::Test::Stub);
TYPE_TO_STRING(test::v1::Test::StubInterface);

TEST_CASE("agrpc::request and agrpc::wait are noexcept for use_sender")
{
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcContext&>()));
    CHECK_FALSE(noexcept(agrpc::request(std::declval<decltype(&test::v1::Test::Stub::PrepareAsyncServerStreaming)>(),
                                        std::declval<test::v1::Test::Stub&>(), std::declval<grpc::ClientContext&>(),
                                        std::declval<test::msg::Request&>(),
                                        std::declval<std::unique_ptr<grpc::ClientAsyncReader<test::msg::Response>>&>(),
                                        std::declval<asio::yield_context>())));
    CHECK(noexcept(agrpc::request(std::declval<decltype(&test::v1::Test::Stub::PrepareAsyncServerStreaming)>(),
                                  std::declval<test::v1::Test::Stub&>(), std::declval<grpc::ClientContext&>(),
                                  std::declval<test::msg::Request&>(),
                                  std::declval<std::unique_ptr<grpc::ClientAsyncReader<test::msg::Response>>&>(),
                                  std::declval<UseSender&&>())));
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
    auto deadline = test::five_seconds_from_now();
    SUBCASE("success") {}
    SUBCASE("deadline expires")
    {
        actual_ok = true;
        expected_ok = false;
        deadline = test::now() - std::chrono::seconds(5);
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
            reader =
                stub->AsyncUnary(&client_context, client_request, agrpc::get_completion_queue(coro.get_executor()));
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

#define choose_client_rpc(Decision, Async, PrepareAsync) \
    []                                                   \
    {                                                    \
        if constexpr (Decision)                          \
        {                                                \
            return &Async;                               \
        }                                                \
        else                                             \
        {                                                \
            return &PrepareAsync;                        \
        }                                                \
    }()

TEST_CASE_TEMPLATE("yield_context server streaming", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    constexpr bool IS_STUB_INTERFACE = std::is_same_v<test::v1::Test::StubInterface, Stub>;
    const auto client_rpc =
        choose_client_rpc(IS_STUB_INTERFACE, Stub::AsyncServerStreaming, Stub::PrepareAsyncServerStreaming);
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    bool use_write_last{false};
    SUBCASE("server write_last") { use_write_last = true; }
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    test::spawn_and_run(
        test.grpc_context,
        [&](const asio::yield_context& yield)
        {
            test::msg::Request request;
            grpc::ServerAsyncWriter<test::msg::Response> writer{&test.server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, test.service,
                                 test.server_context, request, writer, yield));
            test::ServerAsyncWriter<IS_STUB_INTERFACE> writer_ref = writer;
            CHECK(agrpc::send_initial_metadata(writer_ref, yield));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            CHECK(agrpc::write(writer_ref, response, grpc::WriteOptions{}, yield));
            if (use_write_and_finish)
            {
                CHECK(agrpc::write_and_finish(writer_ref, response, {}, grpc::Status::OK, yield));
            }
            else
            {
                if (use_write_last)
                {
                    CHECK(agrpc::write_last(writer_ref, response, grpc::WriteOptions{}, yield));
                }
                else
                {
                    CHECK(agrpc::write(writer_ref, response, yield));
                }
                CHECK(agrpc::finish(writer_ref, grpc::Status::OK, yield));
            }
        },
        [&](const asio::yield_context& yield)
        {
            test::msg::Request request;
            request.set_integer(42);
            auto [reader, ok] = [&]
            {
                if (use_client_convenience)
                {
                    return agrpc::request(client_rpc, test_stub, test.client_context, request, yield);
                }
                test::ClientAsyncReader<IS_STUB_INTERFACE> reader;
                bool ok = agrpc::request(client_rpc, test_stub, test.client_context, request, reader, yield);
                return std::pair{std::move(reader), ok};
            }();
            CHECK(ok);
            CHECK(agrpc::read_initial_metadata(*reader, yield));
            test::msg::Response response;
            CHECK(agrpc::read(*reader, response, yield));
            CHECK(agrpc::read(reader, response, yield));
            grpc::Status status;
            CHECK(agrpc::finish(*reader, status, yield));
            CHECK(status.ok());
            CHECK_EQ(21, response.integer());
        });
}

TEST_CASE_TEMPLATE("yield_context client streaming", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    constexpr bool IS_STUB_INTERFACE = std::is_same_v<test::v1::Test::StubInterface, Stub>;
    const auto client_rpc =
        choose_client_rpc(IS_STUB_INTERFACE, Stub::AsyncClientStreaming, Stub::PrepareAsyncClientStreaming);
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    bool use_write_last{false};
    SUBCASE("client write_last") { use_write_last = true; }
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    test::spawn_and_run(
        test.grpc_context,
        [&](const asio::yield_context& yield)
        {
            grpc::ServerAsyncReader<test::msg::Response, test::msg::Request> reader{&test.server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, test.service,
                                 test.server_context, reader, yield));
            test::ServerAsyncReader<IS_STUB_INTERFACE> reader_ref = reader;
            CHECK(agrpc::send_initial_metadata(reader_ref, yield));
            test::msg::Request request;
            CHECK(agrpc::read(reader_ref, request, yield));
            CHECK_EQ(42, request.integer());
            CHECK(agrpc::read(reader_ref, request, yield));
            CHECK_EQ(42, request.integer());
            CHECK_FALSE(agrpc::read(reader_ref, request, yield));
            test::msg::Response response;
            response.set_integer(21);
            if (use_finish_with_error)
            {
                CHECK(agrpc::finish_with_error(reader_ref, grpc::Status::CANCELLED, yield));
            }
            else
            {
                CHECK(agrpc::finish(reader_ref, response, grpc::Status::OK, yield));
            }
        },
        [&](const asio::yield_context& yield)
        {
            test::msg::Response response;
            auto [writer, ok] = [&]
            {
                if (use_client_convenience)
                {
                    return agrpc::request(client_rpc, test_stub, test.client_context, response, yield);
                }
                test::ClientAsyncWriter<IS_STUB_INTERFACE> writer;
                bool ok = agrpc::request(client_rpc, test_stub, test.client_context, writer, response, yield);
                return std::pair{std::move(writer), ok};
            }();
            CHECK(ok);
            test::client_perform_client_streaming_success(response, *writer, yield,
                                                          {use_finish_with_error, use_write_last});
        });
}

TEST_CASE_TEMPLATE("yield_context unary", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    bool use_finish_with_error{false};
    SUBCASE("server finish_with_error") { use_finish_with_error = true; }
    SUBCASE("server finish with OK") {}
    test::spawn_and_run(
        test.grpc_context,
        [&](const asio::yield_context& yield)
        {
            test::msg::Request request;
            grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&test.server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestUnary, test.service, test.server_context,
                                 request, writer, yield));
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
        },
        [&](const asio::yield_context& yield)
        {
            test::client_perform_unary_success(test.grpc_context, test_stub, yield, {use_finish_with_error});
        });
}

TEST_CASE_TEMPLATE("yield_context bidirectional streaming", Stub, test::v1::Test::Stub, test::v1::Test::StubInterface)
{
    constexpr bool IS_STUB_INTERFACE = std::is_same_v<test::v1::Test::StubInterface, Stub>;
    const auto client_rpc = choose_client_rpc(IS_STUB_INTERFACE, Stub::AsyncBidirectionalStreaming,
                                              Stub::PrepareAsyncBidirectionalStreaming);
    test::GrpcClientServerTest test;
    Stub& test_stub = *test.stub;
    bool use_write_and_finish{false};
    SUBCASE("server write_and_finish") { use_write_and_finish = true; }
    bool use_write_last{false};
    SUBCASE("write_last") { use_write_last = true; }
    bool use_client_convenience{false};
    SUBCASE("client use convenience") { use_client_convenience = true; }
    bool set_initial_metadata_corked{false};
    SUBCASE("client set initial metadata corked") { set_initial_metadata_corked = true; }
    test::spawn_and_run(
        test.grpc_context,
        [&](const asio::yield_context& yield)
        {
            grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request> reader_writer{&test.server_context};
            CHECK(agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, test.service,
                                 test.server_context, reader_writer, yield));
            test::ServerAsyncReaderWriter<IS_STUB_INTERFACE> reader_writer_ref = reader_writer;
            CHECK(agrpc::send_initial_metadata(reader_writer_ref, yield));
            test::msg::Request request;
            CHECK(agrpc::read(reader_writer_ref, request, yield));
            CHECK(agrpc::read(reader_writer_ref, request, yield));
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            CHECK(agrpc::write(reader_writer_ref, response, grpc::WriteOptions{}, yield));
            if (use_write_and_finish)
            {
                CHECK(agrpc::write_and_finish(reader_writer_ref, response, {}, grpc::Status::OK, yield));
            }
            else
            {
                if (use_write_last)
                {
                    CHECK(agrpc::write_last(reader_writer_ref, response, {}, yield));
                }
                else
                {
                    CHECK(agrpc::write(reader_writer_ref, response, yield));
                }
                grpc::Status status{grpc::Status::OK};
                CHECK(agrpc::finish(reader_writer_ref, status, yield));
            }
        },
        [&](const asio::yield_context& yield)
        {
            auto [reader_writer, ok] = [&]
            {
                if (use_client_convenience)
                {
                    return agrpc::request(client_rpc, test_stub, test.client_context, yield);
                }
                if (set_initial_metadata_corked)
                {
                    test.client_context.set_initial_metadata_corked(true);
                    return std::pair{test_stub.AsyncBidirectionalStreaming(
                                         &test.client_context, agrpc::get_completion_queue(test.grpc_context), nullptr),
                                     true};
                }
                test::ClientAsyncReaderWriter<IS_STUB_INTERFACE> reader_writer;
                bool ok = agrpc::request(client_rpc, test_stub, test.client_context, reader_writer, yield);
                return std::pair{std::move(reader_writer), ok};
            }();
            if (!set_initial_metadata_corked)
            {
                CHECK(ok);
                CHECK(agrpc::read_initial_metadata(*reader_writer, yield));
            }
            test::msg::Request request;
            request.set_integer(42);
            CHECK(agrpc::write(reader_writer, request, yield));
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
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "agrpc::request for unary RPCs can be called with a unique_ptr<Stub>")
{
    const auto stub =
        test::v1::Test::NewStub(grpc::CreateChannel("localhost:5049", grpc::InsecureChannelCredentials()));
    grpc::ClientContext client_context;
    test::msg::Request request;
    const auto reader = agrpc::request(&test::v1::Test::Stub::AsyncUnary, stub, client_context, request, grpc_context);
    CHECK(reader);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "agrpc::wait correctly unbinds executor_binder and allocator_binder")
{
    grpc::Alarm alarm;
    std::size_t bytes_allocated{};
    test::CountingAllocator<std::byte> allocator{bytes_allocated};
    auto test = [&](auto token)
    {
        agrpc::wait(alarm, test::ten_milliseconds_from_now(), token);
        grpc_context.run();
        auto expected = sizeof(void*) * 4;
        CHECK_LT(expected, sizeof(token));
        CHECK_GE(expected, bytes_allocated);
    };
#ifdef AGRPC_ASIO_HAS_BIND_ALLOCATOR
    SUBCASE("asio::bind_allocator")
    {
        test(asio::bind_allocator(
            allocator, asio::bind_executor(asio::any_io_executor{grpc_context.get_executor()}, [](auto&&) {})));
    }
    SUBCASE("agrpc::bind_allocator")
    {
#endif
        test(agrpc::bind_allocator(
            allocator, asio::bind_executor(asio::any_io_executor{grpc_context.get_executor()}, [](auto&&) {})));
#ifdef AGRPC_ASIO_HAS_BIND_ALLOCATOR
    }
#endif
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest,
                  "client convenience request function correctly unbinds executor_binder and allocator_binder")
{
    // Setup server streaming
    test::msg::Response response;
    test::msg::Request client_request;
    grpc::ServerAsyncWriter<test::msg::Response> writer{&server_context};
    agrpc::request(
        &test::v1::Test::AsyncService::RequestServerStreaming, service, server_context, client_request, writer,
        asio::bind_executor(grpc_context,
                            [&](bool)
                            {
                                agrpc::finish(writer, grpc::Status::OK, asio::bind_executor(grpc_context, [](bool) {}));
                            }));
    // Perform test
    std::size_t bytes_allocated{};
    test::CountingAllocator<std::byte> allocator{bytes_allocated};
    auto token = agrpc::bind_allocator(
        allocator, asio::bind_executor(asio::any_io_executor{grpc_context.get_executor()}, [](auto&&) {}));
    test::msg::Request request;
    agrpc::request(&test::v1::Test::Stub::PrepareAsyncServerStreaming, *stub, client_context, request, token);
    grpc_context.run();
    auto expected = sizeof(void*) * 4;
    CHECK_LT(expected, sizeof(token));
    CHECK_GE(expected, bytes_allocated);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "RPC step after grpc_context stop")
{
    std::optional<bool> ok;
    test::spawn_and_run(grpc_context,
                        [&](const asio::yield_context& yield)
                        {
                            grpc_context.stop();
                            test::msg::Request request;
                            grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
                            ok = agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context,
                                                request, writer, yield);
                        });
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

template <class Executor, class CompletionToken, class... Function>
auto when_one_bind_executor(const Executor& executor, CompletionToken&& token, Function&&... function)
{
    return asio::experimental::make_parallel_group(
               [&](auto& f)
               {
                   return [&](auto&& t)
                   {
                       return f(asio::bind_executor(executor, std::move(t)));
                   };
               }(function)...)
        .async_wait(asio::experimental::wait_for_one(), std::forward<CompletionToken>(token));
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "cancel grpc::Alarm with parallel_group")
{
    std::array<std::size_t, 2> completion_order;
    std::optional<test::ErrorCode> error_code;
    bool ok{true};
    grpc::Alarm alarm;
    asio::steady_timer timer{get_executor(), std::chrono::milliseconds(100)};
    const auto not_too_exceed = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    when_one_bind_executor(
        get_executor(),
        [&](std::array<std::size_t, 2> actual_completion_order, test::ErrorCode timer_ec, bool wait_ok)
        {
            completion_order = actual_completion_order;
            error_code.emplace(timer_ec);
            ok = wait_ok;
        },
        [&](auto&& t)
        {
            return timer.async_wait(std::move(t));
        },
        [&](auto&& t)
        {
            return agrpc::wait(alarm, test::five_seconds_from_now(), std::move(t));
        });
    grpc_context.run();
    CHECK_GT(not_too_exceed, std::chrono::steady_clock::now());
    CHECK_EQ(0, completion_order[0]);
    CHECK_EQ(1, completion_order[1]);
    CHECK_EQ(test::ErrorCode{}, error_code);
    CHECK_FALSE(ok);
}
#endif