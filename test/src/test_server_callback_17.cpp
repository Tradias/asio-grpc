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
#include "utils/client_context.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/io_context_test.hpp"

#include <agrpc/client_callback.hpp>
#include <agrpc/server_callback.hpp>
#include <agrpc/server_callback_ptr.hpp>

#include <utility>

struct ServerCallbackTest : test::GrpcClientServerCallbackTest, test::IoContextTest
{
    ServerCallbackTest() { run_io_context_detached(); }

    auto make_unary_request()
    {
        test::set_default_deadline(client_context);
        test::msg::Request request;
        test::msg::Response response;
        auto status = agrpc::request(&test::v1::Test::Stub::async::Unary, stub->async(), client_context, request,
                                     response, asio::use_future)
                          .get();
        return std::pair{std::move(status), std::move(response)};
    }
};

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback ptr automatic cancellation")
{
    service.unary = [&](grpc::CallbackServerContext*, const test::msg::Request*,
                        test::msg::Response*) -> grpc::ServerUnaryReactor*
    {
        return agrpc::make_reactor<agrpc::ServerUnaryReactor>(io_context.get_executor())->get();
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback ptr TryCancel")
{
    std::promise<bool> finish_ok;
    asio::steady_timer timer{io_context};
    service.unary = [&](grpc::CallbackServerContext* context, const test::msg::Request*,
                        test::msg::Response*) -> grpc::ServerUnaryReactor*
    {
        auto ptr = agrpc::make_reactor<agrpc::ServerUnaryReactor>(io_context.get_executor());
        auto& rpc = *ptr;
        context->TryCancel();
        timer.expires_after(std::chrono::milliseconds(200));
        timer.async_wait(
            [&, ptr](auto&&)
            {
                ptr->wait_for_finish(
                    [&, ptr](auto&&, bool ok)
                    {
                        finish_ok.set_value(ok);
                    });
            });
        return rpc.get();
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK_FALSE(finish_ok.get_future().get());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback ptr finish successfully")
{
    struct MyReactor : agrpc::ServerUnaryReactorBase
    {
        explicit MyReactor(InitArg init_arg, int integer)
            : agrpc::ServerUnaryReactorBase(std::move(init_arg)), integer_(integer)
        {
        }

        int integer_;
    };
    bool use_wait_for_finish{};
    SUBCASE("wait_for_finish") { use_wait_for_finish = true; }
    SUBCASE("no wait_for_finish") {}
    bool finish_ok{};
    service.unary = [&](grpc::CallbackServerContext*, const test::msg::Request*,
                        test::msg::Response* response) -> grpc::ServerUnaryReactor*
    {
        auto ptr = agrpc::allocate_reactor<MyReactor>(get_allocator(), io_context.get_executor(), 42);
        MyReactor& r = *ptr;
        response->set_integer(r.integer_);
        ptr->initiate_finish(grpc::Status::OK);
        if (use_wait_for_finish)
        {
            ptr->wait_for_finish(
                [&](auto&&, bool ok)
                {
                    finish_ok = ok;
                });
        }
        else
        {
            finish_ok = true;
        }
        return ptr->get();
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
    CHECK_EQ(42, response.integer());
    CHECK(finish_ok);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback ptr read/send_initial_metadata successfully")
{
    bool use_early_finish{};
    SUBCASE("early_finish") { use_early_finish = true; }
    SUBCASE("no early_finish") {}
    bool send_ok{};
    service.unary = [&](grpc::CallbackServerContext* context, const test::msg::Request*,
                        test::msg::Response*) -> grpc::ServerUnaryReactor*
    {
        auto ptr = agrpc::allocate_reactor<agrpc::ServerUnaryReactor>(get_allocator(), io_context.get_executor());
        auto& rpc = *ptr;
        context->AddInitialMetadata("test", "a");
        rpc.initiate_send_initial_metadata();
        rpc.wait_for_send_initial_metadata(
            [&, ptr = use_early_finish ? ptr : agrpc::ReactorPtr<agrpc::ServerUnaryReactor>{}](auto&&, bool ok)
            {
                send_ok = ok;
            });
        return rpc.get();
    };
    test::set_default_deadline(client_context);
    agrpc::ClientUnaryReactor rpc(io_context.get_executor());
    test::msg::Request request;
    test::msg::Response response;
    stub->async()->Unary(&client_context, &request, &response, rpc.get());
    rpc.start();
    CHECK(rpc.wait_for_initial_metadata(asio::use_future).get());
    CHECK_EQ(0, client_context.GetServerInitialMetadata().find("test")->second.compare("a"));
    auto status = rpc.wait_for_finish(asio::use_future).get();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK(send_ok);
    CHECK(allocator_has_been_used());
}
