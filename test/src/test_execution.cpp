// Copyright 2024 Dennis Hezel
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
#include "utils/client_rpc.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/delete_guard.hpp"
#include "utils/doctest.hpp"
#include "utils/exception.hpp"
#include "utils/execution_test.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/server_rpc.hpp"
#include "utils/test.hpp"
#include "utils/time.hpp"

#include <optional>
#include <thread>

TEST_CASE("stdexec asio-grpc fulfills std::execution concepts")
{
    CHECK(stdexec::scheduler<agrpc::GrpcExecutor>);
    using GrpcSender = decltype(std::declval<agrpc::Alarm&>().wait(
        std::declval<std::chrono::system_clock::time_point>(), agrpc::use_sender));
    CHECK(stdexec::sender<GrpcSender>);
    CHECK(stdexec::sender_to<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);

    using ScheduleSender = decltype(stdexec::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(stdexec::sender<ScheduleSender>);
    CHECK(stdexec::sender_to<ScheduleSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "stdexec GrpcExecutor::schedule")
{
    bool invoked{false};
    const auto sender = stdexec::schedule(get_executor());
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  invoked = true;
                                              },
                                              state};
    auto operation_state = stdexec::connect(sender, receiver);
    stdexec::start(operation_state);
    CHECK_FALSE(invoked);
    grpc_context.run();
    CHECK(invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "stdexec GrpcExecutor::schedule from GrpcContext::run")
{
    bool invoked{false};
    test::DeleteGuard guard{};
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[&]
                                              {
                                                  auto& operation_state = guard.emplace_with(
                                                      [&]
                                                      {
                                                          return stdexec::connect(stdexec::schedule(get_executor()),
                                                                                  test::FunctionAsReceiver{[&]
                                                                                                           {
                                                                                                               invoked =
                                                                                                                   true;
                                                                                                           }});
                                                      });
                                                  stdexec::start(operation_state);
                                              },
                                              state};
    auto operation_state = stdexec::connect(stdexec::schedule(get_executor()), receiver);
    stdexec::start(operation_state);
    CHECK_FALSE(invoked);
    grpc_context.run();
    CHECK(invoked);
    CHECK_FALSE(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "stdexec GrpcExecutor::schedule from different thread")
{
    bool invoked{false};
    exec::single_thread_context ctx;
    run(stdexec::let_value(stdexec::schedule(ctx.get_scheduler()),
                           [&]
                           {
                               return stdexec::then(stdexec::schedule(get_executor()),
                                                    [&]
                                                    {
                                                        invoked = true;
                                                    });
                           }));
    CHECK(invoked);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest,
                  "stdexec GrpcExecutor::schedule when already running in GrpcContext thread")
{
    std::thread::id expected_thread_id;
    std::thread::id actual_thread_id;
    exec::single_thread_context ctx;
    grpc_context.work_started();
    stdexec::sync_wait(stdexec::when_all(stdexec::let_value(stdexec::schedule(get_executor()),
                                                            [&]
                                                            {
                                                                return stdexec::then(stdexec::schedule(get_executor()),
                                                                                     [&]
                                                                                     {
                                                                                         grpc_context.work_finished();
                                                                                         actual_thread_id =
                                                                                             std::this_thread::get_id();
                                                                                     });
                                                            }),
                                         stdexec::then(stdexec::schedule(ctx.get_scheduler()),
                                                       [&]
                                                       {
                                                           expected_thread_id = std::this_thread::get_id();
                                                           grpc_context.run();
                                                       })));
    CHECK_EQ(expected_thread_id, actual_thread_id);
}

TEST_CASE_TEMPLATE("ScheduleSender start with shutdown GrpcContext", T, std::true_type, std::false_type)
{
    test::DeleteGuard del;
    test::StatefulReceiverState state;
    test::FunctionAsStatefulReceiver receiver{[](auto&&...) {}, state};
    {
        agrpc::GrpcContext grpc_context;
        agrpc::Alarm alarm{grpc_context};
        const auto sender = [&]
        {
            if constexpr (T::value)
            {
                return stdexec::schedule(grpc_context.get_scheduler());
            }
            else
            {
                return alarm.wait(test::five_seconds_from_now(), agrpc::use_sender);
            }
        };
        std::optional<decltype(stdexec::connect(sender(), receiver))> operation_state;
        auto guard =
            agrpc::detail::ScopeGuard{[&]
                                      {
                                          stdexec::start(operation_state.emplace(stdexec::connect(sender(), receiver)));
                                      }};
        // Ensure that the above operation is started during destruction of the GrpcContext:
        auto& op = del.emplace_with(
            [&]
            {
                return stdexec::connect(test::let_stopped(stdexec::schedule(grpc_context.get_scheduler()),
                                                          [&, g = std::move(guard)]() mutable
                                                          {
                                                              [[maybe_unused]] auto gg = std::move(g);
                                                              return stdexec::just();
                                                          }),
                                        test::FunctionAsReceiver{[]() {}});
            });
        stdexec::start(op);
    }
    CHECK(state.was_done);
    CHECK_FALSE(state.exception);
}

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "stdexec agrpc::Alarm.wait from different thread")
{
    bool invoked{false};
    exec::single_thread_context ctx;
    agrpc::Alarm alarm{grpc_context};
    run(stdexec::let_value(stdexec::schedule(ctx.get_scheduler()),
                           [&]
                           {
                               return stdexec::then(alarm.wait(test::ten_milliseconds_from_now(), agrpc::use_sender),
                                                    [&]()
                                                    {
                                                        invoked = true;
                                                    });
                           }));
    CHECK(invoked);
}

TEST_CASE("stdexec GrpcContext.stop() with pending GrpcSender operation")
{
    bool invoked{false};
    exec::single_thread_context ctx;
    std::optional<agrpc::GrpcContext> grpc_context{std::make_unique<grpc::CompletionQueue>()};
    test::FunctionAsReceiver receiver{[&]()
                                      {
                                          invoked = true;
                                      }};
    agrpc::Alarm alarm{*grpc_context};
    auto op = stdexec::connect(alarm.wait(test::ten_milliseconds_from_now(), agrpc::use_sender), receiver);
    stdexec::start(op);
    grpc_context.reset();
    CHECK_FALSE(invoked);
}

decltype(stdexec::schedule(std::declval<agrpc::GrpcExecutor>())) request_handler_archetype(test::UnaryServerRPC&,
                                                                                           test::msg::Request&);

TEST_CASE("stdexec RegisterSenderRPCHandlerSender fulfills unified executor concepts")
{
    using RegisterSenderRPCHandlerSender = decltype(agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(
        std::declval<agrpc::GrpcContext&>(), std::declval<test::v1::Test::AsyncService&>(),
        &request_handler_archetype));
    CHECK(stdexec::sender<RegisterSenderRPCHandlerSender>);
    CHECK(stdexec::sender_to<RegisterSenderRPCHandlerSender, test::FunctionAsReceiver<test::InvocableArchetype>>);
    CHECK(noexcept(stdexec::connect(std::declval<RegisterSenderRPCHandlerSender>(),
                                    std::declval<test::ConditionallyNoexceptNoOpReceiver<true>>())));
    CHECK_FALSE(noexcept(stdexec::connect(std::declval<RegisterSenderRPCHandlerSender>(),
                                          std::declval<test::ConditionallyNoexceptNoOpReceiver<false>>())));
    CHECK(noexcept(stdexec::connect(std::declval<RegisterSenderRPCHandlerSender>(),
                                    std::declval<const test::ConditionallyNoexceptNoOpReceiver<true>&>())));
    CHECK_FALSE(noexcept(stdexec::connect(std::declval<RegisterSenderRPCHandlerSender>(),
                                          std::declval<const test::ConditionallyNoexceptNoOpReceiver<false>&>())));
    using OperationState =
        stdexec::connect_result_t<RegisterSenderRPCHandlerSender, test::FunctionAsReceiver<test::InvocableArchetype>>;
    CHECK(std::is_invocable_v<decltype(stdexec::start), OperationState&>);
}

template <class RPC>
struct StdexecExecutionClientRPCTest : test::ExecutionTestMixin<test::ClientServerRPCTest<RPC>>
{
    using Base = test::ClientServerRPCTest<RPC>;

    template <class RPCHandler, class... ClientFunctions>
    void register_and_perform_requests(RPCHandler&& handler, ClientFunctions&&... client_functions)
    {
        int counter{};
        this->run(
            agrpc::register_sender_rpc_handler<typename Base::ServerRPC>(this->grpc_context, this->service, handler),
            stdexec::on(this->get_executor(),
                        [&counter, &client_functions, &server_shutdown = this->server_shutdown]() -> exec::task<void>
                        {
                            typename Base::ClientRPC::Request request;
                            typename Base::ClientRPC::Response response;
                            co_await client_functions(request, response);
                            ++counter;
                            if (counter == sizeof...(client_functions))
                            {
                                server_shutdown.initiate();
                            }
                        }())...);
    }
};

TEST_CASE_FIXTURE(StdexecExecutionClientRPCTest<test::UnaryClientRPC>, "stdexec UnaryClientRPC coroutine success")
{
    auto client_func = [&](Request& request, Response& response) -> exec::task<void>
    {
        grpc::ClientContext c;
        test::set_default_deadline(c);
        request.set_integer(42);
        const auto status = co_await request_rpc(c, request, response, agrpc::use_sender);
        CHECK_EQ(42, response.integer());
        CHECK_EQ(grpc::StatusCode::OK, status.error_code());
    };
    register_and_perform_requests(
        [&](ServerRPC& rpc, ServerRPC::Request& request)
        {
            return stdexec::let_value(stdexec::just(ServerRPC::Response{}),
                                      [&](auto& response)
                                      {
                                          response.set_integer(request.integer());
                                          return rpc.finish(response, grpc::Status::OK);
                                      });
        },
        client_func, client_func, client_func);
}

TEST_CASE_FIXTURE(StdexecExecutionClientRPCTest<test::BidirectionalStreamingClientRPC>,
                  "stdexec BidirectionalStreamingClientRPC coroutine success")
{
    auto client_func = [&](Request& request, Response& response) -> exec::task<void>
    {
        auto rpc = create_rpc();
        co_await rpc.start(*stub);
        request.set_integer(42);
        CHECK(co_await rpc.write(request));
        CHECK(co_await rpc.writes_done());
        CHECK(co_await rpc.read(response));
        CHECK_EQ(1, response.integer());
        CHECK_FALSE(co_await rpc.read(response));
        CHECK_EQ(1, response.integer());
        CHECK_EQ(grpc::StatusCode::OK, (co_await rpc.finish()).error_code());
    };
    register_and_perform_requests(
        [&](ServerRPC& rpc) -> exec::task<void>
        {
            Response response;
            response.set_integer(1);
            Request request;
            CHECK(co_await rpc.read(request));
            CHECK_FALSE(co_await rpc.read(request));
            CHECK_EQ(42, request.integer());
            CHECK(co_await rpc.write(response));
            CHECK(co_await rpc.finish(grpc::Status::OK));
        },
        client_func, client_func, client_func);
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "stdexec rpc_handler unary - shutdown server")
{
    auto request_count{0};
    run(stdexec::when_all(
            make_client_unary_request_sender(request_count, 4), make_client_unary_request_sender(request_count, 4),
            make_client_unary_request_sender(request_count, 4), make_client_unary_request_sender(request_count, 4)),
        make_unary_rpc_handler_sender());
    CHECK_EQ(4, request_count);
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest,
                  "stdexec rpc_handler unary - throw exception from rpc handler invocation calls set_error")
{
    auto rpc_handler = agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(grpc_context, service,
                                                                                [&](auto&&...)
                                                                                {
                                                                                    throw test::Exception{};
                                                                                    return stdexec::just();
                                                                                });
    std::exception_ptr error_propagation{};
    run(stdexec::when_all(
            make_client_unary_request_sender(test::hundred_milliseconds_from_now(), &check_status_not_ok),
            make_client_unary_request_sender(test::hundred_milliseconds_from_now(), &check_status_not_ok)),
        stdexec::let_error(std::move(rpc_handler),
                           [&](std::exception_ptr ep)
                           {
                               error_propagation = std::move(ep);
                               return stdexec::just();
                           }));
    REQUIRE(error_propagation);
    CHECK_THROWS_AS(std::rethrow_exception(error_propagation), test::Exception);
}

TEST_CASE_FIXTURE(test::ExecutionClientRPCTest<test::UnaryClientRPC>, "stdexec UnaryClientRPC success")
{
    run(agrpc::register_sender_rpc_handler<ServerRPC>(grpc_context, service,
                                                      [&](ServerRPC& rpc, Request& request)
                                                      {
                                                          CHECK_EQ(1, request.integer());
                                                          return stdexec::let_value(stdexec::just(Response{}),
                                                                                    [&](Response& response)
                                                                                    {
                                                                                        response.set_integer(11);
                                                                                        return rpc.finish(
                                                                                            response, grpc::Status::OK);
                                                                                    });
                                                      }),
        stdexec::just(Request{}, Response{}) |
            stdexec::let_value(
                [&](Request& request, Response& response)
                {
                    request.set_integer(1);
                    return request_rpc(client_context, request, response, agrpc::use_sender);
                }) |
            stdexec::then(
                [&](const grpc::Status& status)
                {
                    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
                    server_shutdown.initiate();
                }));
}

TEST_CASE_FIXTURE(test::ExecutionClientRPCTest<test::UnaryClientRPC>,
                  "stdexec Unary ClientRPC::request automatically finishes rpc on error")
{
    server->Shutdown();
    client_context.set_deadline(test::ten_milliseconds_from_now());
    ClientRPC::Request request;
    ClientRPC::Response response;
    run(stdexec::then(request_rpc(true, client_context, request, response, agrpc::use_sender),
                      [](const auto& status)
                      {
                          const auto status_code = status.error_code();
                          CHECK_MESSAGE((grpc::StatusCode::DEADLINE_EXCEEDED == status_code ||
                                         grpc::StatusCode::UNAVAILABLE == status_code),
                                        status_code);
                      }));
}

TEST_CASE_FIXTURE(test::ExecutionClientRPCTest<test::ClientStreamingClientRPC>,
                  "stdexec ClientStreamingRPC wait_for_done")
{
    bool is_cancelled{true};
    ClientRPC rpc{grpc_context};
    Response response;
    run(agrpc::register_sender_rpc_handler<test::NotifyWhenDoneClientStreamingServerRPC>(
            grpc_context, service,
            [&](test::NotifyWhenDoneClientStreamingServerRPC& rpc)
            {
                return stdexec::when_all(stdexec::then(rpc.wait_for_done(),
                                                       [&]()
                                                       {
                                                           is_cancelled = rpc.context().IsCancelled();
                                                       }),
                                         stdexec::let_value(stdexec::just(Response{}),
                                                            [&](Response& response)
                                                            {
                                                                return rpc.finish(response, grpc::Status::OK);
                                                            }));
            }),
        stdexec::just(Request{}) |
            stdexec::let_value(
                [&](Request& request)
                {
                    return start_rpc(rpc, request, response, agrpc::use_sender);
                }) |
            stdexec::let_value(
                [&](bool)
                {
                    return rpc.finish();
                }) |
            stdexec::then(
                [&](const grpc::Status& status)
                {
                    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
                    server_shutdown.initiate();
                }));
    CHECK_FALSE(is_cancelled);
}

TEST_CASE_FIXTURE(test::ExecutionClientRPCTest<test::UnaryClientRPC>,
                  "stdexec Waiter: initiate alarm -> cancel alarm -> wait returns false")
{
    const auto wait = [](agrpc::Alarm& alarm, auto deadline, auto&&...)
    {
        return alarm.wait(deadline, agrpc::use_sender);
    };
    agrpc::Waiter<void()> waiter;
    agrpc::Alarm alarm{grpc_context};
    run(waiter.initiate(wait, alarm, test::five_seconds_from_now()),
        stdexec::then(stdexec::just(),
                      [&]
                      {
                          CHECK_FALSE(waiter.is_ready());
                          alarm.cancel();
                      }),
        stdexec::then(waiter.wait(),
                      [&]()
                      {
                          CHECK(waiter.is_ready());
                      }));
}

struct StdexecMockTest : test::ExecutionTestMixin<test::MockTest>
{
};

TEST_CASE_FIXTURE(StdexecMockTest, "stdexec mock unary request")
{
    using RPC = test::UnaryInterfaceClientRPC;
    test::set_up_unary_test(*this);
    grpc::ClientContext client_context;
    test::set_default_deadline(client_context);
    RPC::Request request;
    RPC::Response response;
    run(RPC::request(grpc_context, stub, client_context, request, response, agrpc::use_sender) |
        stdexec::then(
            [&](const grpc::Status&)
            {
                CHECK_EQ(42, response.integer());
            }));
}

TEST_CASE_FIXTURE(StdexecMockTest, "stdexec mock server streaming request")
{
    using RPC = test::ServerStreamingInterfaceClientRPC;
    test::set_up_server_streaming_test(*this);
    RPC::Request request;
    RPC::Response response;
    RPC rpc{grpc_context, &test::set_default_deadline};
    run(rpc.start(stub, request, agrpc::use_sender) |
        stdexec::let_value(
            [&](bool ok)
            {
                CHECK(ok);
                return rpc.read(response);
            }) |
        stdexec::then(
            [&](bool ok)
            {
                CHECK(ok);
                CHECK_EQ(42, response.integer());
            }));
}