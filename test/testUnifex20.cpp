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
#include "utils/asioForward.hpp"
#include "utils/asioUtils.hpp"
#include "utils/clientContext.hpp"
#include "utils/doctest.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <agrpc/asioGrpc.hpp>

#include <cstddef>
#include <optional>
#include <thread>

DOCTEST_TEST_SUITE(ASIO_GRPC_TEST_CPP_VERSION)
{
TEST_CASE("unifex asio-grpc fulfills unified executor concepts")
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

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::schedule")
{
    bool is_invoked{false};
    auto sender = unifex::schedule(get_executor());
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  is_invoked = true;
                                              },
                                              state};
    std::optional<unifex::connect_result_t<decltype(sender), decltype(receiver)>> operation_state;
    SUBCASE("connect")
    {
        operation_state.emplace(unifex::connect(sender, receiver));
        unifex::start(*operation_state);
    }
    SUBCASE("submit") { unifex::submit(sender, receiver); }
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::submit from Grpc::Context::run")
{
    bool is_invoked{false};
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  unifex::submit(unifex::schedule(get_executor()),
                                                                 test::FunctionAsReceiver{[&]
                                                                                          {
                                                                                              is_invoked = true;
                                                                                          }});
                                              },
                                              state};
    unifex::submit(unifex::schedule(get_executor()), receiver);
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::submit with allocator")
{
    unifex::submit(unifex::schedule(get_executor()), test::FunctionAsReceiver{[] {}, get_allocator()});
    grpc_context.run();
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::execute")
{
    bool is_invoked{false};
    unifex::execute(get_executor(),
                    [&]
                    {
                        is_invoked = true;
                    });
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::schedule from different thread")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    grpc_context.work_started();
    unifex::sync_wait(unifex::when_all(unifex::let_value(unifex::schedule(ctx.get_scheduler()),
                                                         [&]
                                                         {
                                                             return unifex::then(unifex::schedule(get_executor()),
                                                                                 [&]
                                                                                 {
                                                                                     grpc_context.work_finished();
                                                                                     is_invoked = true;
                                                                                 });
                                                         }),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));
    CHECK(is_invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::schedule when already running in GrpcContext thread")
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

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex agrpc::wait from different thread")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    grpc::Alarm alarm;
    grpc_context.work_started();
    unifex::sync_wait(unifex::when_all(
        unifex::let_value(unifex::schedule(ctx.get_scheduler()),
                          [&]
                          {
                              return unifex::then(agrpc::wait(alarm, test::ten_milliseconds_from_now(), use_sender()),
                                                  [&](bool)
                                                  {
                                                      grpc_context.work_finished();
                                                      is_invoked = true;
                                                  });
                          }),
        unifex::then(unifex::just(),
                     [&]
                     {
                         grpc_context.run();
                     })));
    CHECK(is_invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex cancel agrpc::wait")
{
    bool ok{true};
    grpc::Alarm alarm;
    unifex::sync_wait(unifex::when_all(
        unifex::let_value(unifex::schedule(get_executor()),
                          [&]
                          {
                              return unifex::stop_when(
                                  unifex::then(agrpc::wait(alarm, test::five_seconds_from_now(), use_sender()),
                                               [&](bool wait_ok)
                                               {
                                                   ok = wait_ok;
                                               }),
                                  unifex::just());
                          }),
        unifex::then(unifex::just(),
                     [&]
                     {
                         grpc_context.run();
                     })));
    CHECK_FALSE(ok);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex cancel agrpc::wait before starting")
{
    bool is_invoked{false};
    grpc::Alarm alarm;
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&](bool)
                                              {
                                                  is_invoked = true;
                                              },
                                              state};
    unifex::inplace_stop_source source;
    auto sender = unifex::with_query_value(agrpc::wait(alarm, test::five_seconds_from_now(), use_sender()),
                                           unifex::get_stop_token, source.get_token());
    auto op = unifex::connect(std::move(sender), receiver);
    source.request_stop();
    unifex::start(op);
    grpc_context.run();
    CHECK_FALSE(is_invoked);
    CHECK(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE("unifex GrpcContext.stop() with pending GrpcSender operation")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    std::optional<agrpc::GrpcContext> grpc_context{std::make_unique<grpc::CompletionQueue>()};
    test::FunctionAsReceiver receiver{[&](bool)
                                      {
                                          is_invoked = true;
                                      }};
    grpc::Alarm alarm;
    auto op = unifex::connect(agrpc::wait(alarm, test::ten_milliseconds_from_now(), agrpc::use_sender(*grpc_context)),
                              receiver);
    unifex::start(op);
    grpc_context.reset();
    CHECK_FALSE(is_invoked);
}

struct RepeatedlyRequestTest : test::GrpcClientServerTest
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

TEST_CASE_FIXTURE(RepeatedlyRequestTest, "unifex repeatedly_request unary - shutdown server")
{
    auto request_count{0};
    auto request_sender = make_client_unary_request_sender(request_count, 4);
    unifex::sync_wait(unifex::when_all(unifex::sequence(request_sender, request_sender, request_sender, request_sender),
                                       make_unary_repeatedly_request_sender(),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(RepeatedlyRequestTest, "unifex repeatedly_request unary - client requests stop")
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
    unifex::sync_wait(unifex::when_all(unifex::sequence(make_three_requests_then_stop, request_sender),
                                       std::move(repeater),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(RepeatedlyRequestTest, "unifex repeatedly_request unary - server requests stop")
{
    auto request_count{0};
    auto repeater = unifex::let_value_with_stop_source(
        [&](auto& stop)
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
    unifex::sync_wait(unifex::when_all(request_sender, std::move(repeater),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));
    CHECK_EQ(1, request_count);
}

TEST_CASE_FIXTURE(RepeatedlyRequestTest, "unifex repeatedly_request unary - stop with token before start")
{
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            stop.request_stop();
            return make_unary_repeatedly_request_sender();
        });
    unifex::sync_wait(unifex::when_all(std::move(repeater), unifex::then(unifex::just(),
                                                                         [&]
                                                                         {
                                                                             grpc_context.run();
                                                                         })));
    CHECK_FALSE(allocator_has_been_used());
}

TEST_CASE_FIXTURE(RepeatedlyRequestTest,
                  "unifex repeatedly_request unary - throw exception from request handler calls set_error")
{
    int count{};
    auto repeater = agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, service,
        [&](grpc::ServerContext&, test::msg::Request& request,
            grpc::ServerAsyncResponseWriter<test::msg::Response>& writer)
        {
            ++count;
            if (1 == count)
            {
                throw std::logic_error{"excepted"};
            }
            return handle_unary_request_sender(request, writer);
        },
        use_sender());
    const auto check_status_not_ok = [](auto&&, auto&&, auto&& status)
    {
        CHECK_FALSE(status.ok());
    };
    std::exception_ptr error_propagation{};
    unifex::sync_wait(unifex::when_all(
        unifex::sequence(make_client_unary_request_sender(test::hundred_milliseconds_from_now(), check_status_not_ok),
                         make_client_unary_request_sender(test::hundred_milliseconds_from_now(), check_status_not_ok)),
        unifex::let_error(std::move(repeater),
                          [&](std::exception_ptr ep)
                          {
                              error_propagation = std::move(ep);
                              return unifex::just();
                          }),
        unifex::then(unifex::just(),
                     [&]
                     {
                         grpc_context.run();
                     })));
    CHECK_EQ(1, count);
    REQUIRE(error_propagation);
    CHECK_THROWS_AS(std::rethrow_exception(error_propagation), std::logic_error);
}

struct ServerUnaryRequestContext
{
    grpc::ServerAsyncResponseWriter<test::msg::Response> writer;
    test::msg::Request request;
    test::msg::Response response;

    explicit ServerUnaryRequestContext(grpc::ServerContext& context) : writer(&context) {}
};

#if !UNIFEX_NO_COROUTINES
TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unifex::task unary")
{
    bool server_finish_ok{false};
    bool client_finish_ok{false};
    bool use_submit{false};
    SUBCASE("use submit") { use_submit = true; }
    SUBCASE("use co_await") {}
    unifex::sync_wait(unifex::when_all(
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
                                                  }};
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
        }(),
        unifex::then(unifex::just(),
                     [&]
                     {
                         grpc_context.run();
                     })));
    CHECK(server_finish_ok);
    CHECK(client_finish_ok);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unifex repeatedly_request client streaming")
{
    bool is_shutdown{false};
    auto request_count{0};
    unifex::sync_wait(unifex::when_all(
        agrpc::repeatedly_request(
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
                CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, new_client_context,
                                              writer, response, use_sender()));
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
        }(),
        unifex::then(unifex::just(),
                     [&]
                     {
                         grpc_context.run();
                     })));
    CHECK_EQ(4, request_count);
}
#endif
}