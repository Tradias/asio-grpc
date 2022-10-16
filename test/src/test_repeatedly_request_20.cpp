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
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_generic_client_server_test.hpp"
#include "utils/protobuf.hpp"
#include "utils/rpc.hpp"

#include <agrpc/repeatedly_request.hpp>
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
    test::spawn(self.grpc_context,
                [&](const asio::yield_context& yield)
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
            asio::bind_executor(test.get_executor(),
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
    test::spawn(test.grpc_context,
                [&](const asio::yield_context& yield)
                {
                    while (!is_shutdown)
                    {
                        test::client_perform_client_streaming_success(test_stub, yield);
                    }
                    test.server->Shutdown();
                });
    test.grpc_context.run();
    CHECK_EQ(4, request_count);
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
    wait(alarm, test::five_seconds_from_now(),
         [&](bool)
         {
             invoked = true;
         });
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
inline decltype(asio::execution::schedule(std::declval<agrpc::GrpcExecutor>())) request_handler_archetype(
    grpc::ServerContext&, test::msg::Request&, grpc::ServerAsyncResponseWriter<test::msg::Response>&);

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "RepeatedlyRequestSender fulfills unified executor concepts")
{
    using RepeatedlyRequestSender = decltype(agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service, &request_handler_archetype, use_sender()));
    CHECK(asio::execution::sender<RepeatedlyRequestSender>);
    CHECK(asio::execution::is_sender_v<RepeatedlyRequestSender>);
    CHECK(asio::execution::typed_sender<RepeatedlyRequestSender>);
    CHECK(asio::execution::is_typed_sender_v<RepeatedlyRequestSender>);
    CHECK(asio::execution::sender_to<RepeatedlyRequestSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(asio::execution::is_sender_to_v<RepeatedlyRequestSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(
        asio::execution::is_nothrow_connect_v<RepeatedlyRequestSender, test::ConditionallyNoexceptNoOpReceiver<true>>);
    CHECK_FALSE(
        asio::execution::is_nothrow_connect_v<RepeatedlyRequestSender, test::ConditionallyNoexceptNoOpReceiver<false>>);
    CHECK(asio::execution::is_nothrow_connect_v<RepeatedlyRequestSender,
                                                const test::ConditionallyNoexceptNoOpReceiver<true>&>);
    CHECK_FALSE(asio::execution::is_nothrow_connect_v<RepeatedlyRequestSender,
                                                      const test::ConditionallyNoexceptNoOpReceiver<false>&>);
    using OperationState = asio::execution::connect_result_t<RepeatedlyRequestSender, test::InvocableArchetype>;
    CHECK(asio::execution::operation_state<OperationState>);
    CHECK(asio::execution::is_operation_state_v<OperationState>);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable repeatedly_request unary concurrent requests")
{
    static constexpr auto REQUEST_COUNT = 300;
    auto request_received_count{0};
    auto request_send_count{0};
    std::vector<size_t> completion_order;
    asio::thread_pool thread_pool{4};
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        asio::bind_executor(grpc_context,
                            [&](grpc::ServerContext&, test::msg::Request&,
                                grpc::ServerAsyncResponseWriter<test::msg::Response>& writer) -> asio::awaitable<void>
                            {
                                ++request_received_count;
                                grpc::Alarm alarm;
                                co_await agrpc::wait(alarm, test::five_hundred_milliseconds_from_now());
                                test::msg::Response response;
                                response.set_integer(21);
                                CHECK(co_await agrpc::finish(writer, response, grpc::Status::OK));
                                co_await asio::post(asio::bind_executor(thread_pool, asio::use_awaitable));
                            }));
    for (size_t i = 0; i < REQUEST_COUNT; ++i)
    {
        asio::spawn(grpc_context,
                    [&](const asio::yield_context& yield)
                    {
                        test::PerformUnarySuccessOptions options;
                        options.request_payload = request_send_count;
                        ++request_send_count;
                        test::client_perform_unary_success(grpc_context, *stub, yield, options);
                        completion_order.emplace_back(options.request_payload);
                        if (REQUEST_COUNT == completion_order.size())
                        {
                            grpc_context.stop();
                        }
                    });
    }
    grpc_context.run();
    CHECK_EQ(REQUEST_COUNT, request_received_count);
    REQUIRE_EQ(REQUEST_COUNT, completion_order.size());
}

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
    test::spawn(grpc_context,
                [&](const asio::yield_context& yield)
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
    test::spawn(grpc_context,
                [&](const asio::yield_context& yield)
                {
                    test::client_perform_unary_success(grpc_context, *stub, yield);
                });
    grpc_context.run();
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "awaitable repeatedly_request throw exception from request handler")
{
    agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        asio::bind_executor(grpc_context,
                            [&](grpc::ServerContext&, test::msg::Request&,
                                grpc::ServerAsyncResponseWriter<test::msg::Response>&) -> asio::awaitable<void>
                            {
                                throw std::invalid_argument{"test"};
                            }));
    agrpc::GrpcContext client_grpc_context{std::make_unique<grpc::CompletionQueue>()};
    test::co_spawn(client_grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       test::msg::Request request;
                       client_context.set_deadline(test::hundred_milliseconds_from_now());
                       auto reader = agrpc::request(&test::v1::Test::Stub::AsyncUnary, stub, client_context, request,
                                                    client_grpc_context);
                       test::msg::Response response;
                       grpc::Status status;
                       co_await agrpc::finish(reader, response, status);
                   });
    std::thread t{&agrpc::GrpcContext::run, std::ref(client_grpc_context)};
    CHECK_THROWS_WITH_AS(grpc_context.run(), "test", std::invalid_argument);
    t.join();
}
#endif
#endif