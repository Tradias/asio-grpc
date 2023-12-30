// Copyright 2023 Dennis Hezel
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
#include "utils/client_context.hpp"
#include "utils/client_rpc.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/doctest.hpp"
#include "utils/exception.hpp"
#include "utils/execution_test.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/server_rpc.hpp"

#include <agrpc/asio_grpc.hpp>

TEST_CASE("unifex asio-grpc fulfills std::execution concepts")
{
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcExecutor>()));
    using UseSenderFromGrpcContext = decltype(agrpc::use_sender(std::declval<agrpc::GrpcContext&>()));
    CHECK(std::is_same_v<UseSender, UseSenderFromGrpcContext>);

    using GrpcSender = decltype(std::declval<agrpc::Alarm&>().wait(
        std::declval<std::chrono::system_clock::time_point>(), agrpc::use_sender));
    CHECK(unifex::typed_sender<GrpcSender>);
    CHECK(unifex::is_nothrow_connectable_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);

    using ScheduleSender = decltype(unifex::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(unifex::typed_sender<ScheduleSender>);
    CHECK(unifex::is_nothrow_connectable_v<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "unifex GrpcExecutor::schedule blocking_kind")
{
    CHECK_EQ(unifex::blocking_kind::maybe, unifex::blocking(unifex::schedule(grpc_context.get_scheduler())));
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "unifex cancel agrpc::Alarm.wait")
{
    bool ok{true};
    agrpc::Alarm alarm{grpc_context};
    run(unifex::let_value(unifex::schedule(get_executor()),
                          [&]
                          {
                              return unifex::stop_when(
                                  unifex::let_done(alarm.wait(test::five_seconds_from_now(), agrpc::use_sender),
                                                   [&]()
                                                   {
                                                       ok = false;
                                                       return unifex::just();
                                                   }),
                                  unifex::just());
                          }));
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "unifex cancel agrpc::wait before starting")
{
    bool invoked{false};
    agrpc::Alarm alarm{grpc_context};
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]()
                                              {
                                                  invoked = true;
                                              },
                                              state};
    unifex::inplace_stop_source source;
    auto sender = unifex::with_query_value(alarm.wait(test::five_seconds_from_now(), agrpc::use_sender),
                                           unifex::get_stop_token, source.get_token());
    auto op = unifex::connect(std::move(sender), receiver);
    source.request_stop();
    unifex::start(op);
    grpc_context.run();
    CHECK_FALSE(invoked);
    CHECK(state.was_done);
    CHECK_FALSE(state.exception);
}

inline decltype(unifex::schedule(std::declval<agrpc::GrpcExecutor>())) request_handler_archetype(
    grpc::ServerContext&, test::msg::Request&, grpc::ServerAsyncResponseWriter<test::msg::Response>&);

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "RepeatedlyRequestSender fulfills unified executor concepts")
{
    using RepeatedlyRequestSender = decltype(agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service, &request_handler_archetype, use_sender()));
    CHECK(unifex::sender<RepeatedlyRequestSender>);
    CHECK(unifex::typed_sender<RepeatedlyRequestSender>);
    CHECK(unifex::sender_to<RepeatedlyRequestSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(unifex::is_nothrow_connectable_v<RepeatedlyRequestSender, test::ConditionallyNoexceptNoOpReceiver<true>>);
    CHECK_FALSE(
        unifex::is_nothrow_connectable_v<RepeatedlyRequestSender, test::ConditionallyNoexceptNoOpReceiver<false>>);
    CHECK(unifex::is_nothrow_connectable_v<RepeatedlyRequestSender,
                                           const test::ConditionallyNoexceptNoOpReceiver<true>&>);
    CHECK_FALSE(unifex::is_nothrow_connectable_v<RepeatedlyRequestSender,
                                                 const test::ConditionallyNoexceptNoOpReceiver<false>&>);
    using OperationState =
        unifex::connect_result_t<RepeatedlyRequestSender, test::FunctionAsReceiver<test::InvocableArchetype>>;
    CHECK(std::is_invocable_v<decltype(unifex::start), OperationState&>);
}
TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex repeatedly_request unary - shutdown server")
{
    auto request_count{0};
    auto request_sender = make_client_unary_request_sender(request_count, 4);
    run(unifex::sequence(request_sender, request_sender, request_sender, request_sender),
        make_unary_repeatedly_request_sender());
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex repeatedly_request unary - client requests stop")
{
    auto request_count{0};
    unifex::inplace_stop_source stop;
    auto repeater =
        unifex::with_query_value(make_unary_repeatedly_request_sender(), unifex::get_stop_token, stop.get_token());
    auto request_sender = make_client_unary_request_sender(request_count, std::numeric_limits<int>::max());
    auto make_three_requests_then_stop = unifex::then(unifex::sequence(request_sender, request_sender, request_sender),
                                                      [&]()
                                                      {
                                                          stop.request_stop();
                                                      });
    run(unifex::sequence(make_three_requests_then_stop, request_sender), std::move(repeater));
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex repeatedly_request unary - server requests stop")
{
    auto request_count{0};
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            return unifex::let_done(agrpc::repeatedly_request(
                                        &test::v1::Test::AsyncService::RequestUnary, service,
                                        [&](grpc::ServerContext&, test::msg::Request& request,
                                            grpc::ServerAsyncResponseWriter<test::msg::Response>& writer)
                                        {
                                            stop.request_stop();
                                            return handle_unary_request_sender(request, writer);
                                        },
                                        use_sender()),
                                    []()
                                    {
                                        // Prevent stop request from propagating up
                                        return unifex::just();
                                    });
        });
    auto request_sender = make_client_unary_request_sender(request_count, std::numeric_limits<int>::max());
    run(request_sender, std::move(repeater));
    CHECK_EQ(1, request_count);
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex repeatedly_request unary - stop with token before start")
{
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            stop.request_stop();
            return make_unary_repeatedly_request_sender();
        });
    run(std::move(repeater));
    CHECK_FALSE(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest,
                  "unifex repeatedly_request unary - throw exception from request handler invocation calls set_error")
{
    auto repeatedly_request = agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        [&](auto&&...)
        {
            throw test::Exception{};
            return unifex::just();
        },
        use_sender());
    std::exception_ptr error_propagation{};
    run(unifex::sequence(make_client_unary_request_sender(test::hundred_milliseconds_from_now(), &check_status_not_ok),
                         make_client_unary_request_sender(test::hundred_milliseconds_from_now(), &check_status_not_ok)),
        unifex::let_error(std::move(repeatedly_request),
                          [&](std::exception_ptr ep)
                          {
                              error_propagation = std::move(ep);
                              return unifex::just();
                          }));
    REQUIRE(error_propagation);
    CHECK_THROWS_AS(std::rethrow_exception(error_propagation), test::Exception);
}

#if !UNIFEX_NO_COROUTINES
TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest,
                  "unifex repeatedly_request unary - throw exception from request handler sender")
{
    int count{};
    auto repeatedly_request = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            return agrpc::repeatedly_request(
                &test::v1::Test::AsyncService::RequestUnary, service,
                [&](grpc::ServerContext&, test::msg::Request& request,
                    grpc::ServerAsyncResponseWriter<test::msg::Response>& writer) -> unifex::task<void>
                {
                    ++count;
                    if (count == 1)
                    {
                        throw test::Exception{};
                    }
                    stop.request_stop();
                    co_await handle_unary_request_sender(request, writer);
                },
                use_sender());
        });
    run(unifex::sequence(make_client_unary_request_sender(test::hundred_milliseconds_from_now(), &check_status_not_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok)),
        std::move(repeatedly_request));
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex repeatedly_request unary - keeps request handler alive")
{
    int count{};
    auto repeatedly_request = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            return agrpc::repeatedly_request(
                &test::v1::Test::AsyncService::RequestUnary, service,
                [&](grpc::ServerContext&, test::msg::Request& request,
                    grpc::ServerAsyncResponseWriter<test::msg::Response>& writer) -> unifex::task<void>
                {
                    ++count;
                    if (count == 1)
                    {
                        co_await agrpc::Alarm(grpc_context)
                            .wait(test::two_hundred_milliseconds_from_now(), agrpc::use_sender);
                        count = 42;
                    }
                    else
                    {
                        stop.request_stop();
                    }
                    co_await handle_unary_request_sender(request, writer);
                },
                use_sender());
        });
    auto op = unifex::connect(std::move(repeatedly_request), test::ConditionallyNoexceptNoOpReceiver<true>{});
    op.start();
    run(unifex::when_all(make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok)));
    CHECK_EQ(42, count);
}

struct UnifexClientServerTest : test::ExecutionTestMixin<test::GrpcClientServerTest>
{
};

struct ServerUnaryRequestContext
{
    grpc::ServerAsyncResponseWriter<test::msg::Response> writer;
    test::msg::Request request;
    test::msg::Response response;

    explicit ServerUnaryRequestContext(grpc::ServerContext& context) : writer(&context) {}
};

TEST_CASE_FIXTURE(UnifexClientServerTest, "unifex::task unary")
{
    bool server_finish_ok{false};
    bool client_finish_ok{false};
    run(
        [&]() -> unifex::task<void>
        {
            auto context = std::make_shared<ServerUnaryRequestContext>(server_context);
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context,
                                          context->request, context->writer, use_sender()));
            context->response.set_integer(42);
            server_finish_ok =
                co_await agrpc::finish(context->writer, context->response, grpc::Status::OK, use_sender());
        }(),
        [&]() -> unifex::task<void>
        {
            test::msg::Request request;
            request.set_integer(42);
            auto reader =
                agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, client_context, request, grpc_context);
            test::msg::Response response;
            grpc::Status status;
            client_finish_ok = co_await agrpc::finish(*reader, response, status, use_sender());
        }());
    CHECK(server_finish_ok);
    CHECK(client_finish_ok);
}

TEST_CASE_FIXTURE(UnifexClientServerTest, "unifex repeatedly_request client streaming")
{
    bool is_shutdown{false};
    auto request_count{0};
    run(agrpc::repeatedly_request(
            &test::v1::Test::AsyncService::RequestClientStreaming, service,
            [&](grpc::ServerContext&,
                grpc::ServerAsyncReader<test::msg::Response, test::msg::Request>& reader) -> unifex::task<void>
            {
                test::msg::Request request{};
                CHECK(co_await agrpc::read(reader, request, use_sender()));
                CHECK_EQ(42, request.integer());
                test::msg::Response response{};
                response.set_integer(21);
                ++request_count;
                if (request_count > 3)
                {
                    is_shutdown = true;
                }
                CHECK(co_await agrpc::finish(reader, response, grpc::Status::OK, use_sender()));
            },
            use_sender()),
        [&]() -> unifex::task<void>
        {
            while (!is_shutdown)
            {
                test::msg::Response response;
                grpc::ClientContext new_client_context;
                std::unique_ptr<grpc::ClientAsyncWriter<test::msg::Request>> writer;
                CHECK(co_await agrpc::request(&test::v1::Test::Stub::PrepareAsyncClientStreaming, *stub,
                                              new_client_context, writer, response, use_sender()));
                test::msg::Request request;
                request.set_integer(42);
                CHECK(co_await agrpc::write(*writer, request, use_sender()));
                CHECK(co_await agrpc::writes_done(*writer, use_sender()));
                grpc::Status status;
                CHECK(co_await agrpc::finish(*writer, status, use_sender()));
                CHECK(status.ok());
                CHECK_EQ(21, response.integer());
            }
            server->Shutdown();
        }());
    CHECK_EQ(4, request_count);
}

TEST_CASE_FIXTURE(test::ExecutionClientRPCTest<test::BidirectionalStreamingClientRPC>,
                  "unifex BidirectionalStreamingClientRPC can be cancelled")
{
    const auto with_deadline = [&](std::chrono::system_clock::time_point deadline)
    {
        return unifex::stop_when(
            unifex::then(agrpc::Alarm(grpc_context).wait(deadline, agrpc::use_sender), [](auto&&...) {}));
    };
    const auto not_to_exceed = test::two_seconds_from_now();
    Request request;
    run(agrpc::register_sender_rpc_handler<ServerRPC>(grpc_context, service,
                                                      [&](ServerRPC& rpc)
                                                      {
                                                          return rpc.read(request);
                                                      }),
        [&]() -> unifex::task<void>
        {
            auto rpc = create_rpc();
            co_await rpc.start(*stub);
            Response response;
            co_await (rpc.read(response) | with_deadline(test::now()));
            CHECK_EQ(grpc::StatusCode::CANCELLED, (co_await rpc.finish()).error_code());
            server_shutdown.initiate();
        }());
    CHECK_LT(test::now(), not_to_exceed);
}
#endif

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - client requests stop")
{
    auto request_count{0};
    unifex::inplace_stop_source stop;
    auto repeater = unifex::with_query_value(make_unary_rpc_handler_sender(), unifex::get_stop_token, stop.get_token());
    auto request_sender = make_client_unary_request_sender(request_count, std::numeric_limits<int>::max());
    auto make_three_requests_then_stop = unifex::then(unifex::sequence(request_sender, request_sender, request_sender),
                                                      [&]()
                                                      {
                                                          stop.request_stop();
                                                      });
    run(unifex::sequence(make_three_requests_then_stop, request_sender), std::move(repeater));
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - server requests stop")
{
    auto request_count{0};
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            return unifex::let_done(agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(
                                        grpc_context, service,
                                        [&](test::UnaryServerRPC& rpc, auto& request)
                                        {
                                            stop.request_stop();
                                            return handle_unary_request_sender(rpc, request);
                                        }),
                                    []()
                                    {
                                        // Prevent stop request from propagating up
                                        return unifex::just();
                                    });
        });
    auto request_sender = make_client_unary_request_sender(request_count, std::numeric_limits<int>::max());
    run(request_sender, std::move(repeater));
    CHECK_EQ(1, request_count);
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - stop with token before start")
{
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            stop.request_stop();
            return make_unary_rpc_handler_sender();
        });
    run(std::move(repeater));
    CHECK_FALSE(allocator_has_been_used());
}

#if !UNIFEX_NO_COROUTINES
TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - throw exception from rpc handler sender")
{
    bool is_first{true};
    auto rpc_handler = agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(
        grpc_context, service,
        [&](test::UnaryServerRPC& rpc, auto& request) -> unifex::task<void>
        {
            if (std::exchange(is_first, false))
            {
                throw test::Exception{};
            }
            co_await handle_unary_request_sender(rpc, request);
        });
    const auto not_to_exceed = test::two_seconds_from_now();
    CHECK_THROWS_AS(
        run(unifex::sequence(make_client_unary_request_sender(test::five_seconds_from_now(), &check_status_not_ok),
                             make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok)),
            std::move(rpc_handler)),
        test::Exception);
    CHECK_LT(test::now(), not_to_exceed);
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - keeps rpc handler alive")
{
    int count{};
    auto rpc_handler = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            return agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(
                grpc_context, service,
                [&](test::UnaryServerRPC& rpc, auto& request) -> unifex::task<void>
                {
                    ++count;
                    if (count == 1)
                    {
                        co_await agrpc::Alarm(grpc_context)
                            .wait(test::two_hundred_milliseconds_from_now(), agrpc::use_sender);
                        count = 42;
                    }
                    else
                    {
                        stop.request_stop();
                    }
                    co_await handle_unary_request_sender(rpc, request);
                });
        });
    auto op = unifex::connect(std::move(rpc_handler), test::ConditionallyNoexceptNoOpReceiver<true>{});
    op.start();
    run(unifex::when_all(make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok)));
    CHECK_EQ(42, count);
}
#endif

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "unifex Waiter: initiate alarm -> cancel alarm -> wait returns false")
{
    const auto wait = [](agrpc::Alarm& alarm, auto deadline, auto&&...)
    {
        return alarm.wait(deadline, agrpc::use_sender);
    };
    agrpc::Waiter<void()> waiter;
    agrpc::Alarm alarm{grpc_context};
    run(waiter.initiate(wait, alarm, test::five_seconds_from_now()),
        unifex::then(unifex::just(),
                     [&]
                     {
                         CHECK_FALSE(waiter.is_ready());
                         alarm.cancel();
                     }),
        unifex::then(waiter.wait(),
                     [&]()
                     {
                         CHECK(waiter.is_ready());
                     }));
}