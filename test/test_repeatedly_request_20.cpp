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
#include "utils/grpcGenericClientServerTest.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"

#include <agrpc/repeatedlyRequest.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
struct TypedAwaitableRequestHandler
{
    using Test = test::GrpcClientServerTest;

    template <class RPC, class RequestHandler>
    static auto invoke_repeatedly_request(RPC rpc, test::v1::Test::AsyncService& service, RequestHandler handler)
    {
        return agrpc::repeatedly_request(rpc, service, handler);
    }

    static asio::awaitable<test::msg::Request> read_request(
        grpc::ServerContext&, grpc::ServerAsyncReader<test::msg::Response, test::msg::Request>& reader)
    {
        test::msg::Request request;
        CHECK(co_await agrpc::read(reader, request));
        co_return request;
    }

    static asio::awaitable<bool> write_response(
        grpc::ServerAsyncReader<test::msg::Response, test::msg::Request>& reader, const test::msg::Response& response)
    {
        co_return co_await agrpc::finish(reader, response, grpc::Status::OK);
    }
};

TYPE_TO_STRING(TypedAwaitableRequestHandler);

struct GenericAwaitableRequestHandler
{
    using Test = test::GrpcGenericClientServerTest;

    template <class RPC, class RequestHandler>
    static auto invoke_repeatedly_request(RPC, grpc::AsyncGenericService& service, RequestHandler handler)
    {
        return agrpc::repeatedly_request(service, handler);
    }

    static asio::awaitable<test::msg::Request> read_request(grpc::GenericServerContext&,
                                                            grpc::GenericServerAsyncReaderWriter& reader_writer)
    {
        grpc::ByteBuffer buffer;
        CHECK(co_await agrpc::read(reader_writer, buffer));
        co_return test::grpc_buffer_to_message<test::msg::Request>(buffer);
    }

    static asio::awaitable<bool> write_response(grpc::GenericServerAsyncReaderWriter& reader_writer,
                                                const test::msg::Response& response)
    {
        const auto response_buffer = test::message_to_grpc_buffer(response);
        co_return co_await agrpc::write_and_finish(reader_writer, response_buffer, {}, grpc::Status::OK);
    }
};

TYPE_TO_STRING(GenericAwaitableRequestHandler);

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE_TEMPLATE("awaitable repeatedly_request unary", UsePmrExecutor, std::true_type, std::false_type)
{
    test::GrpcClientServerTest self;
    bool use_server_shutdown{false};
    SUBCASE("shutdown server") { use_server_shutdown = true; }
    SUBCASE("stop GrpcContext") {}
    bool is_shutdown{false};
    auto request_count{0};
    auto executor = [&]
    {
        if constexpr (UsePmrExecutor::value)
        {
            return self.get_pmr_executor();
        }
        else
        {
            return self.get_executor();
        }
    }();
    using Executor = decltype(executor);
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, self.service,
        asio::bind_executor(
            executor,
            [&](grpc::ServerContext&, test::msg::Request& request,
                grpc::ServerAsyncResponseWriter<test::msg::Response>& writer) -> asio::awaitable<void, Executor>
            {
                CHECK_EQ(42, request.integer());
                ++request_count;
                if (request_count > 3)
                {
                    is_shutdown = true;
                }
                test::msg::Response response;
                response.set_integer(21);
                co_await agrpc::finish(writer, response, grpc::Status::OK, asio::use_awaitable_t<Executor>{});
            }));
    asio::spawn(self.grpc_context,
                [&](auto&& yield)
                {
                    while (!is_shutdown)
                    {
                        test::client_perform_unary_success(self.grpc_context, *self.stub, yield);
                    }
                    if (use_server_shutdown)
                    {
                        self.server->Shutdown();
                    }
                    else
                    {
                        self.grpc_context.stop();
                    }
                });
    self.grpc_context.run();
    CHECK_EQ(4, request_count);
}

TEST_CASE_TEMPLATE("awaitable repeatedly_request client streaming", T, TypedAwaitableRequestHandler,
                   GenericAwaitableRequestHandler)
{
    typename T::Test test;
    bool is_shutdown{false};
    auto request_count{0};
    {
        const auto request_handler =
            asio::bind_executor(asio::require(test.get_executor(), asio::execution::allocator(test.get_allocator())),
                                [&](auto& server_context, auto& reader) -> asio::awaitable<void>
                                {
                                    CHECK(co_await agrpc::send_initial_metadata(reader));
                                    const auto request = co_await T::read_request(server_context, reader);
                                    CHECK_EQ(42, request.integer());
                                    ++request_count;
                                    if (request_count > 3)
                                    {
                                        is_shutdown = true;
                                    }
                                    test::msg::Response response;
                                    response.set_integer(21);
                                    CHECK(co_await T::write_response(reader, response));
                                });
        T::invoke_repeatedly_request(&test::v1::Test::AsyncService::RequestClientStreaming, test.service,
                                     request_handler);
    }
    test::v1::Test::Stub test_stub{test.channel};
    asio::spawn(test.grpc_context,
                [&](auto&& yield)
                {
                    while (!is_shutdown)
                    {
                        test::client_perform_client_streaming_success(test_stub, yield);
                    }
                    test.server->Shutdown();
                });
    test.grpc_context.run();
    CHECK_EQ(4, request_count);
    CHECK(test.allocator_has_been_used());
}

template <class Awaitable = asio::awaitable<void>>
auto noop_awaitable_request_handler(agrpc::GrpcContext& grpc_context)
{
    return asio::bind_executor(grpc_context,
                               [&](auto&&...) -> Awaitable
                               {
                                   co_return;
                               });
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable repeatedly_request tracks work correctly")
{
    bool invoked{false};
    grpc::Alarm alarm;
    agrpc::wait(alarm, test::five_seconds_from_now(),
                asio::bind_executor(grpc_context,
                                    [&](bool)
                                    {
                                        invoked = true;
                                    }));
    agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestUnary, service,
                              noop_awaitable_request_handler(grpc_context));
    agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestClientStreaming, service,
                              noop_awaitable_request_handler(grpc_context));
    grpc_context.poll();
    server->Shutdown();
    grpc_context.poll();
    CHECK_FALSE(grpc_context.is_stopped());
    CHECK_FALSE(invoked);
    alarm.Cancel();
    grpc_context.poll();
    CHECK(invoked);
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcClientServerTest, "asio use_sender repeatedly_request unary")
{
    bool is_shutdown{false};
    auto request_count{0};
    test::msg::Response response;
    const auto request_handler = [&](grpc::ServerContext&, test::msg::Request& request,
                                     grpc::ServerAsyncResponseWriter<test::msg::Response>& writer)
    {
        CHECK_EQ(42, request.integer());
        ++request_count;
        if (request_count > 3)
        {
            is_shutdown = true;
        }
        response.set_integer(21);
        return agrpc::finish(writer, response, grpc::Status::OK, use_sender());
    };
    asio::execution::submit(
        agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestUnary, service, request_handler, use_sender()),
        test::FunctionAsReceiver{[&]()
                                 {
                                     CHECK_EQ(4, request_count);
                                 }});
    asio::spawn(grpc_context,
                [&](auto&& yield)
                {
                    while (!is_shutdown)
                    {
                        test::client_perform_unary_success(grpc_context, *stub, yield);
                    }
                    server->Shutdown();
                });
    grpc_context.run();
    CHECK_EQ(4, request_count);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable repeatedly_request cancel keeps request handler alive")
{
    struct RequestHandler
    {
        using executor_type = agrpc::GrpcExecutor;

        executor_type executor;
        bool& is_repeatedly_request_completed;

        asio::awaitable<void> operator()(grpc::ServerContext&, test::msg::Request& request,
                                         grpc::ServerAsyncResponseWriter<test::msg::Response>& writer)
        {
            CHECK_EQ(42, request.integer());
            test::msg::Response response;
            response.set_integer(21);
            co_await agrpc::finish(writer, response, grpc::Status::OK);
            CHECK(is_repeatedly_request_completed);
        }

        auto get_executor() const noexcept { return executor; }
    };
    bool is_repeatedly_request_completed{false};
    asio::cancellation_signal signal;
    agrpc::repeatedly_request(&test::v1::Test::AsyncService::RequestUnary, service,
                              RequestHandler{get_executor(), is_repeatedly_request_completed},
                              asio::bind_cancellation_slot(signal.slot(),
                                                           [&]
                                                           {
                                                               is_repeatedly_request_completed = true;
                                                           }));
    signal.emit(asio::cancellation_type::all);
    asio::spawn(grpc_context,
                [&](auto&& yield)
                {
                    test::client_perform_unary_success(grpc_context, *stub, yield);
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable repeatedly_request throw exception from request handler")
{
    asio::cancellation_signal signal;
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        asio::bind_executor(grpc_context,
                            [&](grpc::ServerContext&, test::msg::Request&,
                                grpc::ServerAsyncResponseWriter<test::msg::Response>&) -> asio::awaitable<void>
                            {
                                throw std::invalid_argument{"test"};
                                co_return;
                            }),
        asio::bind_cancellation_slot(signal.slot(), test::NoOp{}));
    asio::spawn(grpc_context,
                [&](auto&& yield)
                {
                    signal.emit(asio::cancellation_type::all);
                    test::client_perform_unary_unchecked(grpc_context, *stub, yield,
                                                         test::hundred_milliseconds_from_now());
                });
    CHECK_THROWS_WITH_AS(grpc_context.run(), "test", std::invalid_argument);
}
#endif
}
#endif