// Copyright 2021 Dennis Hezel
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
#include <string_view>
#include <thread>

namespace test_asio_grpc
{
TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

TEST_CASE("unifex asio-grpc fulfills unified executor concepts")
{
    using UseScheduler = decltype(agrpc::use_scheduler(std::declval<agrpc::GrpcExecutor>()));
    using UseSchedulerFromGrpcContext = decltype(agrpc::use_scheduler(std::declval<agrpc::GrpcContext&>()));
    CHECK(std::is_same_v<UseScheduler, UseSchedulerFromGrpcContext>);
    using Sender =
        decltype(agrpc::wait(std::declval<grpc::Alarm&>(), std::declval<std::chrono::system_clock::time_point>(),
                             std::declval<UseScheduler>()));
    CHECK(unifex::sender<Sender>);
    CHECK(unifex::typed_sender<Sender>);
    CHECK(unifex::sender_to<Sender, test::FunctionAsReciever<test::InvocableArchetype>>);
    using OperationState = unifex::connect_result_t<Sender, test::FunctionAsReciever<test::InvocableArchetype>>;
    CHECK(unifex::scheduler<agrpc::GrpcExecutor>);
}

TEST_CASE_FIXTURE(test::GrpcContextTest, "unifex GrpcExecutor::schedule")
{
    bool is_invoked{false};
    auto sender = unifex::schedule(get_executor());
    test::FunctionAsReciever receiver{[&]
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
            struct Context
            {
                grpc::ServerAsyncResponseWriter<test::v1::Response> writer;
                test::v1::Request request;
                test::v1::Response response;

                explicit Context(grpc::ServerContext& context) : writer(&context) {}
            };
            auto context = std::make_shared<Context>(server_context);
            CHECK(co_await agrpc::request(&test::v1::Test::AsyncService::RequestUnary, service, server_context,
                                          context->request, context->writer, use_scheduler()));
            context->response.set_integer(42);
            if (use_submit)
            {
                test::FunctionAsReciever receiver{[&, context = context](bool ok)
                                                  {
                                                      server_finish_ok = ok;
                                                  }};
                unifex::submit(agrpc::finish(context->writer, context->response, grpc::Status::OK, use_scheduler()),
                               std::move(receiver));
            }
            else
            {
                server_finish_ok =
                    co_await agrpc::finish(context->writer, context->response, grpc::Status::OK, use_scheduler());
            }
        }(),
        [&]() -> unifex::task<void>
        {
            test::v1::Request request;
            request.set_integer(42);
            auto reader = stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(get_executor()));
            test::v1::Response response;
            grpc::Status status;
            client_finish_ok = co_await agrpc::finish(*reader, response, status, use_scheduler());
        }(),
        [&]() -> unifex::task<void>
        {
            grpc_context.run();
            co_return;
        }()));
    CHECK(server_finish_ok);
    CHECK(client_finish_ok);
}
#endif

TEST_SUITE_END();
}  // namespace test_asio_grpc