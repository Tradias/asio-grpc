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

#include "protos/test.grpc.pb.h"
#include "utils/asioUtils.hpp"
#include "utils/grpcClientServerTest.hpp"

#include <agrpc/repeatedlyRequest.hpp>
#include <agrpc/rpcs.hpp>
#include <doctest/doctest.h>

namespace test_repeatedly_request_20
{
TEST_SUITE_BEGIN(ASIO_GRPC_TEST_CPP_VERSION* doctest::timeout(180.0));

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
TEST_CASE_TEMPLATE("repeatedly_request unary with awaitable", T, std::true_type, std::false_type)
{
    test::GrpcClientServerTest self;
    bool use_server_shutdown{false};
    SUBCASE("shutdown server") { use_server_shutdown = true; }
    SUBCASE("stop GrpcContext") {}
    bool is_shutdown{false};
    auto request_count{0};
    test::v1::Response response;
    auto executor = [&]
    {
        if constexpr (T{})
        {
            return asio::require(self.get_executor(), asio::execution::allocator(self.get_allocator()));
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
            [&](grpc::ServerContext&, test::v1::Request& request,
                grpc::ServerAsyncResponseWriter<test::v1::Response>& writer) -> asio::awaitable<void, Executor>
            {
                CHECK_EQ(42, request.integer());
                ++request_count;
                if (request_count > 3)
                {
                    is_shutdown = true;
                }
                response.set_integer(21);
                co_await agrpc::finish(writer, response, grpc::Status::OK, asio::use_awaitable_t<Executor>{});
            }));
    test::co_spawn(self.grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       while (!is_shutdown)
                       {
                           grpc::ClientContext new_client_context;
                           test::v1::Request request;
                           request.set_integer(42);
                           const auto reader = self.stub->AsyncUnary(&new_client_context, request,
                                                                     self.grpc_context.get_completion_queue());
                           test::v1::Response response;
                           grpc::Status status;
                           CHECK(co_await agrpc::finish(*reader, response, status));
                           CHECK(status.ok());
                           CHECK_EQ(21, response.integer());
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
    if constexpr (T{})
    {
        CHECK(self.allocator_has_been_used());
    }
}

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
TEST_CASE_FIXTURE(test::GrpcClientServerTest, "repeatedly_request with asio use_sender")
{
    bool is_shutdown{false};
    auto request_count{0};
    test::v1::Response response;
    asio::execution::submit(agrpc::repeatedly_request(
                                &test::v1::Test::AsyncService::RequestUnary, service,
                                [&](grpc::ServerContext&, test::v1::Request& request,
                                    grpc::ServerAsyncResponseWriter<test::v1::Response>& writer)
                                {
                                    CHECK_EQ(42, request.integer());
                                    ++request_count;
                                    if (request_count > 3)
                                    {
                                        is_shutdown = true;
                                    }
                                    response.set_integer(21);
                                    return agrpc::finish(writer, response, grpc::Status::OK, use_sender());
                                },
                                use_sender()),
                            test::FunctionAsReceiver{[&]()
                                                     {
                                                         CHECK_EQ(4, request_count);
                                                     }});
    test::co_spawn(grpc_context,
                   [&]() -> asio::awaitable<void>
                   {
                       while (!is_shutdown)
                       {
                           grpc::ClientContext new_client_context;
                           test::v1::Request request;
                           request.set_integer(42);
                           const auto reader =
                               stub->AsyncUnary(&new_client_context, request, grpc_context.get_completion_queue());
                           test::v1::Response response;
                           grpc::Status status;
                           CHECK(co_await agrpc::finish(*reader, response, status));
                           CHECK(status.ok());
                           CHECK_EQ(21, response.integer());
                       }
                       server->Shutdown();
                   });
    grpc_context.run();
    CHECK_EQ(4, request_count);
}
#endif
#endif

TEST_SUITE_END();
}  // namespace test_asio_grpc