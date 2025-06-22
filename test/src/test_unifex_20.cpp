// Copyright 2025 Dennis Hezel
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

#include "utils/asio_utils.hpp"
#include "utils/client_rpc.hpp"
#include "utils/client_rpc_test.hpp"
#include "utils/doctest.hpp"
#include "utils/exception.hpp"
#include "utils/execution_test.hpp"
#include "utils/protobuf.hpp"
#include "utils/requestMessageFactory.hpp"
#include "utils/server_rpc.hpp"

#include <agrpc/asio_grpc.hpp>

TEST_CASE("unifex asio-grpc fulfills std::execution concepts")
{
    CHECK(unifex::scheduler<agrpc::GrpcExecutor>);
    using GrpcSender = decltype(std::declval<agrpc::Alarm&>().wait(
        std::declval<std::chrono::system_clock::time_point>(), agrpc::use_sender));
    CHECK(unifex::sender<GrpcSender>);
    CHECK(unifex::is_nothrow_connectable_v<GrpcSender, test::FunctionAsReceiver<test::InvocableArchetype>>);

    using ScheduleSender = decltype(unifex::schedule(std::declval<agrpc::GrpcExecutor>()));
    CHECK(unifex::sender<ScheduleSender>);
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

TEST_CASE_FIXTURE(test::ExecutionGrpcContextTest, "unifex cancel agrpc::Alarm.wait before starting")
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

#if !UNIFEX_NO_COROUTINES
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
    run(agrpc::register_sender_rpc_handler<ServerRPC>(
            grpc_context, service,
            test::RPCHandlerWithRequestMessageFactory{[&](ServerRPC& rpc)
                                                      {
                                                          return rpc.read(request, agrpc::use_sender);
                                                      }}),
        [&]() -> unifex::task<void>
        {
            auto rpc = create_rpc();
            co_await rpc.start(*stub, agrpc::use_sender);
            Response response;
            co_await (rpc.read(response, agrpc::use_sender) | with_deadline(test::now()));
            CHECK_EQ(grpc::StatusCode::CANCELLED, (co_await rpc.finish(agrpc::use_sender)).error_code());
            server_shutdown.initiate();
        }());
    CHECK_LT(test::now(), not_to_exceed);
}
#endif

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - client requests stop")
{
    auto request_count{0};
    unifex::inplace_stop_source stop;
    auto rpc_handler_sender =
        unifex::with_query_value(make_unary_rpc_handler_sender(), unifex::get_stop_token, stop.get_token());
    auto request_sender = make_client_unary_request_sender(request_count, std::numeric_limits<int>::max());
    auto make_three_requests_then_stop = unifex::then(unifex::sequence(request_sender, request_sender, request_sender),
                                                      [&]()
                                                      {
                                                          stop.request_stop();
                                                      });
    run(unifex::sequence(make_three_requests_then_stop, request_sender), std::move(rpc_handler_sender));
    CHECK_EQ(4, request_count);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - server requests stop")
{
    auto request_count{0};
    auto rpc_handler_sender = unifex::let_value_with_stop_source(
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
    run(request_sender, std::move(rpc_handler_sender));
    CHECK_EQ(1, request_count);
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary with request message factory")
{
    auto rpc_handler_sender = agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(
        grpc_context, service,
        test::RPCHandlerWithRequestMessageFactory{
            [&](test::UnaryServerRPC& rpc, auto& request, test::ArenaRequestMessageFactory& factory)
            {
                CHECK_EQ(42, request.integer());
                CHECK(test::has_arena(request, factory.arena));
                return handle_unary_request_sender(rpc, request);
            }});
    run(make_client_unary_request_sender(test::five_seconds_from_now(),
                                         [&](const test::msg::Response& response, const grpc::Status& status)
                                         {
                                             check_response_ok(response, status);
                                             shutdown.initiate();
                                         }),
        std::move(rpc_handler_sender));
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest, "unifex rpc_handler unary - stop with token before start")
{
    auto rpc_handler_sender = unifex::let_value_with_stop_source(
        [&](unifex::inplace_stop_source& stop)
        {
            stop.request_stop();
            return make_unary_rpc_handler_sender();
        });
    run(std::move(rpc_handler_sender));
    CHECK_FALSE(allocator_has_been_used());
}

TEST_CASE_FIXTURE(test::ExecutionRpcHandlerTest,
                  "unifex rpc_handler unary - throw exception from request message factory")
{
    struct RPCHandler
    {
        auto operator()(test::UnaryServerRPC& rpc, test::UnaryServerRPC::Request& request,
                        test::ArenaRequestMessageFactory&)
        {
            return test.handle_unary_request_sender(rpc, request);
        }

        test::ArenaRequestMessageFactory request_message_factory()
        {
            if (counter++ == throw_counter)
            {
                throw test::Exception{};
            }
            return {};
        }

        test::ExecutionRpcHandlerTest& test;
        int throw_counter;
        int counter{};
    };
    int counter{};
    SUBCASE("throw on first request") {}
    SUBCASE("throw on second request") { counter = 1; }
    std::exception_ptr eptr;
    auto rpc_handler = unifex::let_error(
        agrpc::register_sender_rpc_handler<test::UnaryServerRPC>(grpc_context, service, RPCHandler{*this, counter}),
        [&](auto&& ep)
        {
            eptr = ep;
            return unifex::just();
        });
    run(unifex::stop_when(unifex::sequence(make_client_unary_request_sender<false>(test::five_seconds_from_now()),
                                           make_client_unary_request_sender<false>(test::five_seconds_from_now())),
                          std::move(rpc_handler)));
    CHECK_THROWS_AS(std::rethrow_exception(eptr), test::Exception);
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
        unifex::then(waiter.wait(agrpc::use_sender),
                     [&]()
                     {
                         CHECK(waiter.is_ready());
                     }));
}

#if defined(AGRPC_TEST_ASIO_HAS_CORO) && !UNIFEX_NO_COROUTINES
struct UnifexCoroutineTraits
{
    using ReturnType = unifex::task<void>;

    template <class RPCHandler, class CompletionHandler>
    static auto completion_token(RPCHandler&, CompletionHandler&)
    {
        return agrpc::use_sender;
    }

    template <class RPCHandler, class CompletionHandler, class IoExecutor, class Function>
    static void co_spawn(const IoExecutor& scheduler, RPCHandler& handler, CompletionHandler&, Function&& function)
    {
        handler.scope_.detached_spawn_on(scheduler, static_cast<Function&&>(function)());
    }
};

TEST_CASE_FIXTURE(test::ExecutionClientRPCTest<test::ClientStreamingClientRPC>,
                  "unifex ClientStreamingRPC with register_coroutine_rpc_handler")
{
    unifex::async_scope scope;
    struct Handler
    {
        unifex::task<void> operator()(ServerRPC& rpc) const
        {
            Request request;
            co_await rpc.read(request, agrpc::use_sender);
            CHECK_EQ(1, request.integer());
            Response response;
            response.set_integer(11);
            co_await rpc.finish(response, grpc::Status::OK, agrpc::use_sender);
        }

        unifex::async_scope& scope_;
    };
    agrpc::register_coroutine_rpc_handler<ServerRPC, UnifexCoroutineTraits>(grpc_context, service, Handler{scope},
                                                                            test::RethrowFirstArg{});
    run(scope.complete(),
        [&]() -> unifex::task<void>
        {
            auto rpc = create_rpc();
            Response response;
            co_await rpc.start(*stub, response, agrpc::use_sender);
            Request request;
            request.set_integer(1);
            co_await rpc.write(request, agrpc::use_sender);
            CHECK_EQ(grpc::StatusCode::OK, (co_await rpc.finish(agrpc::use_sender)).error_code());
            CHECK_EQ(11, response.integer());
            server_shutdown.initiate();
        }());
}
#endif