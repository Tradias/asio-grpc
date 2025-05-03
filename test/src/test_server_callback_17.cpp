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
    using Request = test::msg::Request;
    using Response = test::msg::Response;

    ServerCallbackTest() { run_io_context_detached(); }

    auto make_unary_request()
    {
        test::set_default_deadline(client_context);
        Request request;
        Response response;
        auto status = agrpc::request(&test::v1::Test::Stub::async::Unary, stub->async(), client_context, request,
                                     response, asio::use_future)
                          .get();
        return std::pair{std::move(status), std::move(response)};
    }
};

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback ptr automatic cancellation")
{
    service.unary = [&](grpc::CallbackServerContext*, const Request*, Response*) -> grpc::ServerUnaryReactor*
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
    service.unary = [&](grpc::CallbackServerContext* context, const Request*, Response*) -> grpc::ServerUnaryReactor*
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
    bool use_wait_for_finish{};
    SUBCASE("wait_for_finish") { use_wait_for_finish = true; }
    SUBCASE("no wait_for_finish") {}
    bool finish_ok{};
    service.unary = [&](grpc::CallbackServerContext*, const Request*, Response* response) -> grpc::ServerUnaryReactor*
    {
        struct MyReactor : agrpc::ServerUnaryReactorBase
        {
            explicit MyReactor(InitArg init_arg, int integer)
                : agrpc::ServerUnaryReactorBase(std::move(init_arg)), integer_(integer)
            {
            }

            int integer_;
        };
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
    service.unary = [&](grpc::CallbackServerContext* context, const Request*, Response*) -> grpc::ServerUnaryReactor*
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
    auto rpc = agrpc::make_reactor<agrpc::ClientUnaryReactor>(io_context.get_executor());
    test::set_default_deadline(rpc->context());
    Request request;
    Response response;
    rpc->start(&test::v1::Test::Stub::async::Unary, stub->async(), request, response);
    CHECK(rpc->wait_for_initial_metadata(asio::use_future).get());
    CHECK_EQ(0, rpc->context().GetServerInitialMetadata().find("test")->second.compare("a"));
    auto status = rpc->wait_for_finish(asio::use_future).get();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK(send_ok);
    CHECK(allocator_has_been_used());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Client-streaming callback ptr")
{
    Request server_request;
    service.client_streaming = [&](grpc::CallbackServerContext*, Response*) -> grpc::ServerReadReactor<Request>*
    {
        auto ptr = agrpc::make_reactor<agrpc::ServerReadReactor<Request>>(io_context.get_executor());
        auto& rpc = *ptr;
        rpc.initiate_read(server_request);
        rpc.wait_for_read(
            [&, ptr](auto&&, bool ok)
            {
                CHECK(ok);
                CHECK_EQ(1, server_request.integer());
                rpc.initiate_read(server_request);
                rpc.wait_for_read(
                    [&, ptr](auto&&, bool ok)
                    {
                        CHECK(ok);
                        CHECK_EQ(2, server_request.integer());
                        rpc.initiate_finish(grpc::Status::OK);
                    });
            });
        return rpc.get();
    };
    auto rpc = agrpc::make_reactor<agrpc::ClientWriteReactor<Request>>(io_context.get_executor());
    test::set_default_deadline(rpc->context());
    Response response;
    rpc->start(&test::v1::Test::Stub::async::ClientStreaming, stub->async(), response);
    Request request;
    request.set_integer(1);
    rpc->initiate_write(request);
    CHECK(rpc->wait_for_write(asio::use_future).get());
    request.set_integer(2);
    rpc->initiate_write(request);
    CHECK(rpc->wait_for_write(asio::use_future).get());
    auto status = rpc->wait_for_finish(asio::use_future).get();
    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Client-streaming callback ptr cancel after write")
{
    Request server_request;
    service.client_streaming = [&](grpc::CallbackServerContext*, Response*) -> grpc::ServerReadReactor<Request>*
    {
        auto ptr = agrpc::make_reactor<agrpc::ServerReadReactor<Request>>(io_context.get_executor());
        auto& rpc = *ptr;
        rpc.initiate_read(server_request);
        rpc.wait_for_read(
            [&, ptr](auto&&, bool ok)
            {
                CHECK(ok);
                CHECK_EQ(1, server_request.integer());
                rpc.initiate_read(server_request);
                rpc.wait_for_read(
                    [&, ptr](auto&&, bool ok)
                    {
                        CHECK_FALSE(ok);
                    });
            });
        return rpc.get();
    };
    auto rpc = agrpc::make_reactor<agrpc::ClientWriteReactor<Request>>(io_context.get_executor());
    test::set_default_deadline(rpc->context());
    Response response;
    rpc->start(&test::v1::Test::Stub::async::ClientStreaming, stub->async(), response);
    Request request;
    request.set_integer(1);
    rpc->initiate_write(request);
    CHECK(rpc->wait_for_write(asio::use_future).get());
    rpc->context().TryCancel();
    rpc->initiate_write(request);
    CHECK_FALSE(rpc->wait_for_write(asio::use_future).get());
    auto status = rpc->wait_for_finish(asio::use_future).get();
    rpc->wait_for_finish(asio::use_future).get();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
}
