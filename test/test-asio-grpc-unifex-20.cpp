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

#include "agrpc/asioGrpc.hpp"
#include "protos/test.grpc.pb.h"
#include "utils/asioForward.hpp"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <thread>

namespace test_asio_grpc_unifex_cpp20
{
TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

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
    test::FunctionAsReceiver receiver{[&]
                                      {
                                          is_invoked = true;
                                      }};
    std::optional<unifex::connect_result_t<decltype(sender), decltype(receiver)>> operation_state;
    SUBCASE("connect")
    {
        operation_state.emplace(unifex::connect(std::move(sender), receiver));
        unifex::start(*operation_state);
    }
    SUBCASE("submit") { unifex::submit(std::move(sender), receiver); }
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
    CHECK_FALSE(receiver.was_done);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::submit from Grpc::Context::run")
{
    bool is_invoked{false};
    test::FunctionAsReceiver receiver{[&]
                                      {
                                          unifex::submit(unifex::schedule(get_executor()),
                                                         test::FunctionAsReceiver{[&]
                                                                                  {
                                                                                      is_invoked = true;
                                                                                  }});
                                      }};
    unifex::submit(unifex::schedule(get_executor()), receiver);
    CHECK_FALSE(is_invoked);
    grpc_context.run();
    CHECK(is_invoked);
    CHECK_FALSE(receiver.was_done);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::submit with allocator")
{
    unifex::submit(unifex::schedule(get_executor()), test::FunctionAsReceiver{[] {}, get_allocator()});
    grpc_context.run();
    CHECK(std::any_of(buffer.begin(), buffer.end(),
                      [](auto&& value)
                      {
                          return value != std::byte{};
                      }));
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

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex submit to stopped GrpcContext")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    unifex::sync_wait(unifex::let_value(unifex::schedule(ctx.get_scheduler()),
                                        [&]
                                        {
                                            grpc_context.stop();
                                            return unifex::then(unifex::schedule(get_executor()),
                                                                [&]
                                                                {
                                                                    is_invoked = true;
                                                                });
                                        }));
    grpc_context.run();
    CHECK_FALSE(is_invoked);
}

TEST_CASE("unifex GrpcContext.stop() with pending ScheduleSender operation")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    std::optional<agrpc::GrpcContext> grpc_context{std::make_unique<grpc::CompletionQueue>()};
    auto receiver = test::FunctionAsReceiver{[&]
                                             {
                                                 is_invoked = true;
                                             }};
    auto op = unifex::connect(unifex::schedule(grpc_context->get_scheduler()), receiver);
    unifex::start(op);
    grpc_context.reset();
    CHECK_FALSE(is_invoked);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex agrpc::wait with stopped GrpcContext")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    grpc::Alarm alarm;
    unifex::sync_wait(unifex::let_value(unifex::schedule(ctx.get_scheduler()),
                                        [&]
                                        {
                                            grpc_context.stop();
                                            return unifex::then(
                                                agrpc::wait(alarm, test::ten_milliseconds_from_now(), use_sender()),
                                                [&](bool)
                                                {
                                                    is_invoked = true;
                                                });
                                        }));
    grpc_context.run();
    CHECK_FALSE(is_invoked);
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

TEST_CASE("unifex GrpcContext.stop() with pending GrpcSender operation")
{
    bool is_invoked{false};
    unifex::new_thread_context ctx;
    std::optional<agrpc::GrpcContext> grpc_context{std::make_unique<grpc::CompletionQueue>()};
    auto receiver = test::FunctionAsReceiver{[&](bool)
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

template <class Handler, class Allocator = std::allocator<std::byte>>
struct AssociatedHandler
{
    Handler handler;
    Allocator allocator;

    explicit AssociatedHandler(Handler handler, Allocator allocator) : handler(handler), allocator(allocator) {}

    template <class... Args>
    auto operator()(Args&&... args) const
    {
        return handler(std::forward<Args>(args)...);
    }

    friend auto tag_invoke(unifex::tag_t<unifex::get_allocator>, const AssociatedHandler& receiver) noexcept
    {
        return receiver.allocator;
    }
};

auto make_client_unary_request_sender(test::GrpcClientServerTest& self, int& request_count, int max_request_count)
{
    return unifex::let_value_with(
        [&]
        {
            auto context = std::make_unique<grpc::ClientContext>();
            test::v1::Request request;
            request.set_integer(42);
            auto* context_ptr = context.get();
            return std::tuple{
                self.stub->AsyncUnary(context_ptr, request, agrpc::get_completion_queue(self.get_executor())),
                test::v1::Response{}, grpc::Status{}, std::move(context)};
        },
        [&, max_request_count](auto& tuple)
        {
            auto& [reader, response, status, _] = tuple;
            return unifex::then(agrpc::finish(*reader, response, status, self.use_sender()),
                                [&, max_request_count](bool ok)
                                {
                                    auto& [reader, response, status, _] = tuple;
                                    CHECK(ok);
                                    CHECK(status.ok());
                                    CHECK_EQ(24, response.integer());
                                    ++request_count;
                                    if (request_count == max_request_count)
                                    {
                                        unifex::execute(self.get_executor(),
                                                        [&]
                                                        {
                                                            self.server->Shutdown();
                                                        });
                                    }
                                });
        });
}

auto make_unary_repeatedly_request_sender(test::GrpcClientServerTest& self)
{
    return agrpc::repeatedly_request(
        &test::v1::Test::AsyncService::RequestUnary, self.service,
        AssociatedHandler{[&](grpc::ServerContext&, test::v1::Request& request,
                              grpc::ServerAsyncResponseWriter<test::v1::Response>& writer)
                          {
                              CHECK_EQ(42, request.integer());
                              return unifex::let_value(unifex::just(test::v1::Response{}),
                                                       [&](auto& response)
                                                       {
                                                           response.set_integer(24);
                                                           return agrpc::finish(writer, response, grpc::Status::OK,
                                                                                self.use_sender());
                                                       });
                          },
                          self.get_allocator()},
        self.use_sender());
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unifex repeatedly_request unary - shutdown server")
{
    auto request_count{0};
    auto request_sender = make_client_unary_request_sender(*this, request_count, 4);
    unifex::sync_wait(unifex::when_all(unifex::sequence(request_sender, request_sender, request_sender, request_sender),
                                       make_unary_repeatedly_request_sender(*this),
                                       unifex::then(unifex::just(),
                                                    [&]
                                                    {
                                                        grpc_context.run();
                                                    })));
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unifex repeatedly_request unary - stop token")
{
    auto request_count{0};
    unifex::inplace_stop_source* stop_source{};
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            stop_source = &stop;
            return make_unary_repeatedly_request_sender(*this);
        });
    auto request_sender = make_client_unary_request_sender(*this, request_count, std::numeric_limits<int>::max());
    auto make_three_requests_then_stop = unifex::then(unifex::sequence(request_sender, request_sender, request_sender),
                                                      [&]()
                                                      {
                                                          stop_source->request_stop();
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

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unifex repeatedly_request unary - stop before start")
{
    auto repeater = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            stop.request_stop();
            return make_unary_repeatedly_request_sender(*this);
        });
    unifex::sync_wait(unifex::when_all(std::move(repeater), unifex::then(unifex::just(),
                                                                         [&]
                                                                         {
                                                                             grpc_context.run();
                                                                         })));
    CHECK_FALSE(allocator_has_been_used());
}

struct ServerUnaryRequestContext
{
    grpc::ServerAsyncResponseWriter<test::v1::Response> writer;
    test::v1::Request request;
    test::v1::Response response;

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
            test::v1::Request request;
            request.set_integer(42);
            auto reader = stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(get_executor()));
            test::v1::Response response;
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
            AssociatedHandler{
#ifdef _MSC_VER
                [&](grpc::ServerContext&, grpc::ServerAsyncReader<test::v1::Response, test::v1::Request>& reader)
                {
                    return unifex::let_value(
                        unifex::just(test::v1::Request{}),
                        [&](auto& request)
                        {
                            return unifex::let_value(
                                agrpc::read(reader, request, use_sender()),
                                [&](bool read_ok)
                                {
                                    CHECK(read_ok);
                                    CHECK_EQ(42, request.integer());
                                    return unifex::let_value(
                                        unifex::just(test::v1::Response{}),
                                        [&](auto& response)
                                        {
                                            response.set_integer(21);
                                            ++request_count;
                                            if (request_count > 3)
                                            {
                                                is_shutdown = true;
                                            }
                                            return unifex::then(
                                                agrpc::finish(reader, response, grpc::Status::OK, use_sender()),
                                                [](bool finish_ok)
                                                {
                                                    CHECK(finish_ok);
                                                });
                                        });
                                });
                        });
                },
#else
                [&](grpc::ServerContext&,
                    grpc::ServerAsyncReader<test::v1::Response, test::v1::Request>& reader) -> unifex::task<void>
                {
                    test::v1::Request request{};
                    CHECK(co_await agrpc::read(reader, request, use_sender()));
                    CHECK_EQ(42, request.integer());
                    test::v1::Response response{};
                    response.set_integer(21);
                    ++request_count;
                    if (request_count > 3)
                    {
                        is_shutdown = true;
                    }
                    CHECK(co_await agrpc::finish(reader, response, grpc::Status::OK, use_sender()));
                },
#endif
                get_allocator()},
            use_sender()),
        [&]() -> unifex::task<void>
        {
            while (!is_shutdown)
            {
                test::v1::Response response;
                grpc::ClientContext new_client_context;
                std::unique_ptr<grpc::ClientAsyncWriter<test::v1::Request>> writer;
                CHECK(co_await agrpc::request(&test::v1::Test::Stub::AsyncClientStreaming, *stub, new_client_context,
                                              writer, response, use_sender()));
                test::v1::Request request;
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
    CHECK(allocator_has_been_used());
}
#endif

TEST_SUITE_END();
}  // namespace test_asio_grpc_unifex_cpp20