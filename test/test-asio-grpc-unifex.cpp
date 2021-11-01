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
#include "utils/grpcClientServerTest.hpp"
#include "utils/grpcContextTest.hpp"

#include <doctest/doctest.h>
#include <grpcpp/alarm.h>

#include <cstddef>
#include <string_view>
#include <thread>

namespace test_asio_grpc
{
using namespace agrpc;

TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

#if !UNIFEX_NO_COROUTINES
TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unifex::task unary")
{
    grpc_context.work_started();
    grpc_context.work_started();
    grpc_context.work_started();
    bool server_finish_ok = false;
    bool client_finish_ok = false;
    unifex::sync_wait(unifex::when_all(
        [&]() -> unifex::task<void>
        {
            test::v1::Request request;
            grpc::ServerAsyncResponseWriter<test::v1::Response> writer{&server_context};
            CHECK(co_await agrpc::async_request(get_executor(), &test::v1::Test::AsyncService::RequestUnary, service,
                                                server_context, request, writer));
            test::v1::Response response;
            response.set_integer(42);
            server_finish_ok = co_await agrpc::async_finish(get_executor(), writer, response, grpc::Status::OK);
        }(),
        [&]() -> unifex::task<void>
        {
            test::v1::Request request;
            request.set_integer(42);
            auto reader = stub->AsyncUnary(&client_context, request, agrpc::get_completion_queue(get_executor()));
            test::v1::Response response;
            grpc::Status status;
            client_finish_ok = co_await agrpc::async_finish(get_executor(), *reader, response, status);
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