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
#include "utils/asio_forward.hpp"
#include "utils/asio_utils.hpp"
#include "utils/client_context.hpp"
#include "utils/delete_guard.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/high_level_client.hpp"

#include <agrpc/asio_grpc.hpp>

#include <cstddef>
#include <optional>
#include <thread>

struct UnifexTest : virtual test::GrpcContextTest
{
    template <class... Sender>
    void run(Sender&&... sender)
    {
        grpc_context.work_started();
        unifex::sync_wait(unifex::when_all(unifex::finally(unifex::when_all(std::forward<Sender>(sender)...),
                                                           unifex::then(unifex::just(),
                                                                        [&]
                                                                        {
                                                                            grpc_context.work_finished();
                                                                        })),
                                           unifex::then(unifex::just(),
                                                        [&]
                                                        {
                                                            grpc_context.run();
                                                        })));
    }
};

TEST_CASE("unifex asio-grpc fulfills std::execution concepts")
{
    CHECK(unifex::scheduler<agrpc::GrpcExecutor>);
    using UseSender = decltype(agrpc::use_sender(std::declval<agrpc::GrpcExecutor>()));
    using UseSenderFromGrpcContext = decltype(agrpc::use_sender(std::declval<agrpc::GrpcContext&>()));
    CHECK(std::is_same_v<UseSender, UseSenderFromGrpcContext>);
    using GrpcSender =
        decltype(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                             std::declval<UseSender>()));
    CHECK(unifex::sender<GrpcSender>);
    CHECK(unifex::typed_sender<GrpcSender>);
    CHECK(unifex::sender_to<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(unifex::is_nothrow_connectable_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);

    using ScheduleSender = decltype(unifex::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(unifex::sender<ScheduleSender>);
    CHECK(unifex::typed_sender<ScheduleSender>);
    CHECK(unifex::sender_to<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(unifex::is_nothrow_connectable_v<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex GrpcExecutor::schedule")
{
    bool invoked{false};
    test::DeleteGuard guard{};
    const auto sender = unifex::schedule(get_executor());
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  invoked = true;
                                              },
                                              state};
    SUBCASE("connect")
    {
        auto& operation_state = guard.emplace_with(
            [&]
            {
                return unifex::connect(sender, receiver);
            });
        unifex::start(operation_state);
    }
    SUBCASE("submit") { unifex::submit(sender, receiver); }
    CHECK_FALSE(invoked);
    grpc_context.run();
    CHECK(invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex GrpcExecutor::submit from Grpc::Context::run")
{
    bool invoked{false};
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  unifex::submit(unifex::schedule(get_executor()),
                                                                 test::FunctionAsReceiver{[&]
                                                                                          {
                                                                                              invoked = true;
                                                                                          }});
                                              },
                                              state};
    unifex::submit(unifex::schedule(get_executor()), receiver);
    CHECK_FALSE(invoked);
    grpc_context.run();
    CHECK(invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex GrpcExecutor::submit with allocator")
{
    unifex::submit(unifex::schedule(get_executor()), test::FunctionAsReceiver{test::NoOp{}, get_allocator()});
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(UnifexTest, "unifex GrpcExecutor::execute")
{
    bool invoked{false};
    unifex::execute(get_executor(),
                    [&]
                    {
                        invoked = true;
                    });
    CHECK_FALSE(invoked);
    grpc_context.run();
    CHECK(invoked);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex GrpcExecutor::schedule from different thread")
{
    bool invoked{false};
    unifex::new_thread_context ctx;
    run(unifex::let_value(unifex::schedule(ctx.get_scheduler()),
                          [&]
                          {
                              return unifex::then(unifex::schedule(get_executor()),
                                                  [&]
                                                  {
                                                      invoked = true;
                                                  });
                          }));
    CHECK(invoked);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex GrpcExecutor::schedule when already running in GrpcContext thread")
{
    std::thread::id expected_thread_id;
    std::thread::id actual_thread_id;
    unifex::new_thread_context ctx;
    grpc_context.work_started();
    unifex::sync_wait(unifex::when_all(unifex::let_value(unifex::schedule(get_executor()),
                                                         [&]
                                                         {
                                                             return unifex::then(unifex::schedule(get_executor()),
                                                                                 [&]
                                                                                 {
                                                                                     grpc_context.work_finished();
                                                                                     actual_thread_id =
                                                                                         std::this_thread::get_id();
                                                                                 });
                                                         }),
                                       unifex::then(unifex::schedule(ctx.get_scheduler()),
                                                    [&]
                                                    {
                                                        expected_thread_id = std::this_thread::get_id();
                                                        grpc_context.run();
                                                    })));
    CHECK_EQ(expected_thread_id, actual_thread_id);
}

#if !UNIFEX_NO_COROUTINES
TEST_CASE_TEMPLATE("ScheduleSender start/submit with shutdown GrpcContext", T, std::true_type, std::false_type)
{
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[](auto&&...) {}, state};
    {
        agrpc::GrpcContext grpc_context;
        grpc::Alarm alarm;
        const auto sender = [&]
        {
            if constexpr (T::value)
            {
                return unifex::schedule(grpc_context.get_scheduler());
            }
            else
            {
                return agrpc::wait(alarm, test::five_seconds_from_now(), agrpc::use_sender(grpc_context));
            }
        };
        std::optional<decltype(unifex::connect(sender(), receiver))> operation_state;
        auto guard = agrpc::detail::ScopeGuard{[&]
                                               {
                                                   SUBCASE("submit") { unifex::submit(sender(), receiver); }
                                                   SUBCASE("start")
                                                   {
                                                       operation_state.emplace(unifex::connect(sender(), receiver));
                                                       unifex::start(*operation_state);
                                                   }
                                               }};
        unifex::submit(unifex::let_value(unifex::schedule(grpc_context.get_scheduler()),
                                         [&]
                                         {
                                             grpc_context.stop();
                                             return agrpc::wait(alarm, test::five_seconds_from_now(),
                                                                agrpc::use_sender(grpc_context));
                                         }),
                       test::FunctionAsReceiver{[guard = std::move(guard)](bool) {}});
        grpc_context.run();
    }
    CHECK(state.was_done);
    CHECK_FALSE(state.exception);
}
#endif

TEST_CASE_FIXTURE(UnifexTest, "unifex agrpc::wait from different thread")
{
    bool invoked{false};
    unifex::new_thread_context ctx;
    grpc::Alarm alarm;
    run(unifex::let_value(unifex::schedule(ctx.get_scheduler()),
                          [&]
                          {
                              return unifex::then(agrpc::wait(alarm, test::ten_milliseconds_from_now(), use_sender()),
                                                  [&](bool)
                                                  {
                                                      invoked = true;
                                                  });
                          }));
    CHECK(invoked);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex cancel agrpc::wait")
{
    bool ok{true};
    grpc::Alarm alarm;
    run(unifex::let_value(unifex::schedule(get_executor()),
                          [&]
                          {
                              return unifex::stop_when(
                                  unifex::then(agrpc::wait(alarm, test::five_seconds_from_now(), use_sender()),
                                               [&](bool wait_ok)
                                               {
                                                   ok = wait_ok;
                                               }),
                                  unifex::just());
                          }));
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(UnifexTest, "unifex cancel agrpc::wait before starting")
{
    bool invoked{false};
    grpc::Alarm alarm;
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&](bool)
                                              {
                                                  invoked = true;
                                              },
                                              state};
    unifex::inplace_stop_source source;
    auto sender = unifex::with_query_value(agrpc::wait(alarm, test::five_seconds_from_now(), use_sender()),
                                           unifex::get_stop_token, source.get_token());
    auto op = unifex::connect(std::move(sender), receiver);
    source.request_stop();
    unifex::start(op);
    grpc_context.run();
    CHECK_FALSE(invoked);
    CHECK(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE("unifex GrpcContext.stop() with pending GrpcSender operation")
{
    bool invoked{false};
    unifex::new_thread_context ctx;
    std::optional<agrpc::GrpcContext> grpc_context{std::make_unique<grpc::CompletionQueue>()};
    test::FunctionAsReceiver receiver{[&](bool)
                                      {
                                          invoked = true;
                                      }};
    grpc::Alarm alarm;
    auto op = unifex::connect(agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::use_sender(*grpc_context)),
                              receiver);
    unifex::start(op);
    grpc_context.reset();
    CHECK_FALSE(invoked);
}

struct UnifexRepeatedlyRequestTest : UnifexTest, test::GrpcClientServerTest
{
    template <class OnRequestDone = test::NoOp>
    auto make_client_unary_request_sender(std::chrono::system_clock::time_point deadline,
                                          OnRequestDone on_request_done = {})
    {
        return unifex::let_value_with(
            [&, deadline]
            {
                auto context = test::create_client_context(deadline);
                test::msg::Request request;
                request.set_integer(42);
                auto* context_ptr = context.get();
                return std::tuple{
                    agrpc::request(&test::v1::Test::Stub::AsyncUnary, *stub, *context_ptr, request, grpc_context),
                    test::msg::Response{}, grpc::Status{}, std::move(context)};
            },
            [&, on_request_done](auto& tuple)
            {
                auto& [reader, response, status, _] = tuple;
                return unifex::then(agrpc::finish(*reader, response, status, use_sender()),
                                    [&, on_request_done](bool ok) mutable
                                    {
                                        auto& [reader, response, status, _] = tuple;
                                        std::move(on_request_done)(ok, response, status);
                                    });
            });
    }

    static void check_response_ok(bool ok, const test::msg::Response& response, const grpc::Status& status)
    {
        CHECK(ok);
        CHECK(status.ok());
        CHECK_EQ(24, response.integer());
    }

    static void check_status_not_ok(bool, const test::msg::Response&, const grpc::Status& status)
    {
        CHECK_FALSE(status.ok());
    }

    auto make_client_unary_request_sender(int& request_count, int max_request_count)
    {
        return make_client_unary_request_sender(test::five_seconds_from_now(),
                                                [&, max_request_count](bool ok, auto&& response, auto&& status)
                                                {
                                                    check_response_ok(ok, response, status);
                                                    ++request_count;
                                                    if (request_count == max_request_count)
                                                    {
                                                        unifex::execute(get_executor(),
                                                                        [&]
                                                                        {
                                                                            server->Shutdown();
                                                                        });
                                                    }
                                                });
    }

    auto handle_unary_request_sender(test::msg::Request& request,
                                     grpc::ServerAsyncResponseWriter<test::msg::Response>& writer)
    {
        CHECK_EQ(42, request.integer());
        return unifex::let_value(unifex::just(test::msg::Response{}),
                                 [&](auto& response)
                                 {
                                     response.set_integer(24);
                                     return agrpc::finish(writer, response, grpc::Status::OK, use_sender());
                                 });
    }

    auto make_unary_repeatedly_request_sender()
    {
        return unifex::with_query_value(agrpc::repeatedly_request(
                                            &test::v1::Test::AsyncService::RequestUnary, service,
                                            [&](grpc::ServerContext&, test::msg::Request& request,
                                                grpc::ServerAsyncResponseWriter<test::msg::Response>& writer)
                                            {
                                                return handle_unary_request_sender(request, writer);
                                            },
                                            use_sender()),
                                        unifex::get_allocator, get_allocator());
    }
};

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

TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest, "unifex repeatedly_request unary - shutdown server")
{
    auto request_count{0};
    auto request_sender = make_client_unary_request_sender(request_count, 4);
    run(unifex::sequence(request_sender, request_sender, request_sender, request_sender),
        make_unary_repeatedly_request_sender());
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest, "unifex repeatedly_request unary - client requests stop")
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

TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest, "unifex repeatedly_request unary - server requests stop")
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

TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest, "unifex repeatedly_request unary - stop with token before start")
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

TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest,
                  "unifex repeatedly_request unary - throw exception from request handler invocation calls set_error")
{
    auto repeatedly_request = agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        [&](auto&&...)
        {
            throw std::logic_error{"excepted"};
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
    CHECK_THROWS_AS(std::rethrow_exception(error_propagation), std::logic_error);
}

#if !UNIFEX_NO_COROUTINES
TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest,
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
                        throw std::logic_error{"excepted"};
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

TEST_CASE_FIXTURE(UnifexRepeatedlyRequestTest, "unifex repeatedly_request unary - keeps request handler alive")
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
                        co_await agrpc::Alarm(grpc_context).wait(test::two_hundred_milliseconds_from_now());
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
    unifex::submit(std::move(repeatedly_request), test::ConditionallyNoexceptNoOpReceiver<true>{});
    run(unifex::when_all(make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok),
                         make_client_unary_request_sender(test::five_seconds_from_now(), &check_response_ok)));
    CHECK_EQ(42, count);
}

struct UnifexClientServerTest : UnifexTest, test::GrpcClientServerTest
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
    bool use_submit{false};
    SUBCASE("use submit") { use_submit = true; }
    SUBCASE("use co_await") {}
    run(
        [&]() -> unifex::task<void>
        {
            auto context = std::make_shared<ServerUnaryRequestContext>(server_context);
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context,
                                          context->request, context->writer, use_sender()));
            context->response.set_integer(42);
            if (use_submit)
            {
                test::FunctionAsReceiver receiver{[&, context = context](bool ok)
                                                  {
                                                      server_finish_ok = ok;
                                                  },
                                                  get_allocator()};
                unifex::submit(agrpc::finish(context->writer, context->response, grpc::Status::OK, use_sender()),
                               std::move(receiver));
            }
            else
            {
                server_finish_ok =
                    co_await agrpc::finish(context->writer, context->response, grpc::Status::OK, use_sender());
            }
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
    if (use_submit)
    {
        CHECK(allocator_has_been_used());
    }
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

struct UnifexHighLevelTest : test::HighLevelClientTest<test::BidirectionalStreamingRPC>, UnifexTest
{
};

TEST_CASE_FIXTURE(UnifexHighLevelTest, "unifex high-level client BidirectionalStreamingRPC success")
{
    run(
        [&]() -> unifex::task<void>
        {
            CHECK(co_await test_server.request_rpc(use_sender()));
            test_server.response.set_integer(1);
            CHECK(co_await agrpc::read(test_server.responder, test_server.request, use_sender()));
            CHECK_FALSE(co_await agrpc::read(test_server.responder, test_server.request, use_sender()));
            CHECK_EQ(42, test_server.request.integer());
            CHECK(co_await agrpc::write(test_server.responder, test_server.response, use_sender()));
            CHECK(co_await agrpc::finish(test_server.responder, grpc::Status::OK, use_sender()));
        }(),
        [&]() -> unifex::task<void>
        {
            auto rpc = co_await test::BidirectionalStreamingRPC::request(grpc_context, *stub, client_context);
            request.set_integer(42);
            CHECK(co_await rpc.write(request));
            CHECK(co_await rpc.writes_done());
            CHECK(co_await rpc.read(response));
            CHECK_EQ(1, response.integer());
            CHECK(co_await rpc.writes_done());
            CHECK_FALSE(co_await rpc.read(response));
            CHECK_EQ(1, response.integer());
            CHECK(co_await rpc.finish());
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
            CHECK(co_await rpc.finish());
            CHECK_EQ(grpc::StatusCode::OK, rpc.status_code());
        }());
}
#endif