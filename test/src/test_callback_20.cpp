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
#include "utils/exception.hpp"
#include "utils/server_callback_test.hpp"

#include <agrpc/client_callback.hpp>
#include <agrpc/reactor_ptr.hpp>
#include <agrpc/server_callback.hpp>
#include <agrpc/server_callback_coroutine.hpp>

using ServerCallbackTest = test::ServerCallbackTest;

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback coroutine automatic cancellation")
{
    service.unary = [](grpc::CallbackServerContext*, const Request*, Response*) -> grpc::ServerUnaryReactor*
    {
        co_return;
    };
    Request request;
    Response response;
    std::promise<grpc::Status> p;
    agrpc::request(&test::v1::Test::Stub::async::Unary, stub->async(), client_context, request, response,
                   [&](auto&& status)
                   {
                       p.set_value(status);
                   });
    CHECK_EQ(grpc::StatusCode::CANCELLED, p.get_future().get().error_code());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback coroutine TryCancel")
{
    std::promise<bool> finish_ok;
    asio::steady_timer timer{io_context};
    service.unary = [&](grpc::CallbackServerContext* context, const Request*, Response*) -> grpc::ServerUnaryReactor*
    {
        context->TryCancel();
        timer.expires_after(std::chrono::milliseconds(200));
        co_await timer.async_wait(asio::deferred);
        finish_ok.set_value(co_await agrpc::wait_for_finish);
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK_FALSE(finish_ok.get_future().get());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback coroutine exception")
{
    service.unary = [&](grpc::CallbackServerContext*, const Request*, Response*) -> grpc::ServerUnaryReactor*
    {
        co_await agrpc::get_reactor;
        throw test::Exception{};
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::INTERNAL, status.error_code());
    CHECK_EQ("Unhandled exception", status.error_message());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback coroutine finish successfully")
{
    bool use_wait_for_finish{};
    SUBCASE("wait_for_finish") { use_wait_for_finish = true; }
    SUBCASE("no wait_for_finish") {}
    std::promise<bool> finish_ok;
    service.unary = [&](grpc::CallbackServerContext*, const Request*, Response* response) -> grpc::ServerUnaryReactor*
    {
        response->set_integer(42);
        co_await agrpc::initiate_finish(grpc::Status::OK);
        if (use_wait_for_finish)
        {
            finish_ok.set_value(co_await agrpc::wait_for_finish);
        }
        else
        {
            finish_ok.set_value(true);
        }
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
    CHECK_EQ(42, response.integer());
    CHECK(finish_ok.get_future().get());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Unary callback coroutine read/send_initial_metadata successfully")
{
    std::promise<bool> send_ok;
    service.unary = [&](grpc::CallbackServerContext* context, const Request*, Response*) -> grpc::ServerUnaryReactor*
    {
        context->AddInitialMetadata("test", test::to_string(context->client_metadata().find("test")->second));
        co_await agrpc::initiate_send_initial_metadata;
        bool ok = co_await agrpc::wait_for_send_initial_metadata;
        send_ok.set_value(ok);
        server_done();
    };
    auto rpc = agrpc::make_reactor<agrpc::ClientUnaryReactor>(io_context.get_executor());
    test::set_default_deadline(rpc->context());
    rpc->context().AddMetadata("test", "a");
    Request request;
    Response response;
    rpc->start(&test::v1::Test::Stub::async::Unary, stub->async(), request, response);
    CHECK(rpc->wait_for_initial_metadata(asio::use_future).get());
    CHECK_EQ(0, rpc->context().GetServerInitialMetadata().find("test")->second.compare("a"));
    auto status = rpc->wait_for_finish(asio::use_future).get();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    CHECK(send_ok.get_future().get());
}

TEST_CASE_FIXTURE(ServerCallbackTest, "Client-streaming callback coroutine")
{
    Request server_request;
    service.client_streaming = [&](grpc::CallbackServerContext*, Response*) -> grpc::ServerReadReactor<Request>*
    {
        auto& reactor = co_await agrpc::get_reactor;
        reactor.initiate_read(server_request);
        bool ok = co_await reactor.wait_for_read(asio::deferred);
        CHECK(ok);
        CHECK_EQ(1, server_request.integer());
        reactor.initiate_read(server_request);
        ok = co_await reactor.wait_for_read(asio::deferred);
        CHECK(ok);
        CHECK_EQ(2, server_request.integer());
        reactor.initiate_finish(grpc::Status::OK);
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

TEST_CASE_FIXTURE(ServerCallbackTest, "Client-streaming callback coroutine cancel after write")
{
    Request server_request;
    service.client_streaming = [&](grpc::CallbackServerContext*, Response*) -> grpc::ServerReadReactor<Request>*
    {
        co_await agrpc::initiate_read(server_request);
        bool ok = co_await agrpc::wait_for_read;
        server_done();
        CHECK(ok);
        CHECK_EQ(1, server_request.integer());
        co_await agrpc::initiate_read(server_request);
        ok = co_await agrpc::wait_for_read;
        CHECK_FALSE(ok);
    };
    auto rpc = agrpc::make_reactor<agrpc::ClientWriteReactor<Request>>(io_context.get_executor());
    test::set_default_deadline(rpc->context());
    Response response;
    rpc->start(&test::v1::Test::Stub::async::ClientStreaming, stub->async(), response);
    Request request;
    request.set_integer(1);
    rpc->initiate_write(request);
    CHECK(rpc->wait_for_write(asio::use_future).get());
    wait_for_server_done();
    rpc->context().TryCancel();
    rpc->initiate_write(request);
    CHECK_FALSE(rpc->wait_for_write(asio::use_future).get());
    auto status = rpc->wait_for_finish(asio::use_future).get();
    rpc->wait_for_finish(asio::use_future).get();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
}
