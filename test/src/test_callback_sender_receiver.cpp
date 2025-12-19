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
#include "utils/execution_utils.hpp"
#include "utils/grpc_client_server_callback_test.hpp"

#include <agrpc/client_callback.hpp>
#include <agrpc/reactor_ptr.hpp>
#include <agrpc/server_callback.hpp>

#include <future>

struct ServerCallbackStdexecTest : test::GrpcClientServerCallbackTest
{
    using Request = test::msg::Request;
    using Response = test::msg::Response;

    ServerCallbackStdexecTest() { test::set_default_deadline(client_context); }

    auto make_unary_request()
    {
        Request request;
        Response response;
        auto status = test::sync_wait(agrpc::unary_call(&test::v1::Test::Stub::async::Unary, stub->async(),
                                                        client_context, request, response, agrpc::use_sender));
        return std::pair{*std::move(status), std::move(response)};
    }

    void wait_for_server_done() { server_done_promise.get_future().wait(); }

    void server_done() { server_done_promise.set_value(); }

    Request client_request;
    Response client_response;
    Request server_request;
    Response server_response;
    std::promise<void> server_done_promise;
};

TEST_CASE_FIXTURE(ServerCallbackStdexecTest, "stdexec Unary callback coroutine automatic cancellation")
{
    service.unary = [](grpc::CallbackServerContext*, const Request*, Response*) -> grpc::ServerUnaryReactor*
    {
        auto ptr = agrpc::make_reactor<agrpc::BasicServerUnaryReactor<void>>();
        return ptr->get();
    };
    Request request;
    Response response;
    auto status = test::sync_wait(agrpc::unary_call(&test::v1::Test::Stub::async::Unary, stub->async(), client_context,
                                                    request, response, agrpc::use_sender));
    CHECK_EQ(grpc::StatusCode::CANCELLED, status->error_code());
}

TEST_CASE_FIXTURE(ServerCallbackStdexecTest, "stdexec Unary callback coroutine TryCancel")
{
    bool finish_ok;
    exec::async_scope scope;
    service.unary = [&](grpc::CallbackServerContext* context, const Request*, Response*) -> grpc::ServerUnaryReactor*
    {
        auto ptr = agrpc::make_reactor<agrpc::BasicServerUnaryReactor<void>>();
        auto& rpc = *ptr;
        context->TryCancel();
        test::scope_spawn_detached(scope, stdexec::then(rpc.wait_for_finish(agrpc::use_sender),
                                                        [&, ptr = std::move(ptr)](bool ok)
                                                        {
                                                            finish_ok = ok;
                                                        }));
        return rpc.get();
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::CANCELLED, status.error_code());
    test::sync_wait(test::scope_on_empty(scope));
    CHECK_FALSE(finish_ok);
}

TEST_CASE_FIXTURE(ServerCallbackStdexecTest, "stdexec Unary callback coroutine finish successfully")
{
    bool use_wait_for_finish{};
    SUBCASE("wait_for_finish") { use_wait_for_finish = true; }
    SUBCASE("no wait_for_finish") {}
    exec::async_scope scope;
    bool finish_ok;
    service.unary = [&](grpc::CallbackServerContext*, const Request*, Response* response) -> grpc::ServerUnaryReactor*
    {
        auto ptr = agrpc::make_reactor<agrpc::BasicServerUnaryReactor<void>>();
        response->set_integer(42);
        auto& rpc = *ptr;
        ptr->initiate_finish(grpc::Status::OK);
        if (use_wait_for_finish)
        {
            test::scope_spawn_detached(scope, stdexec::then(rpc.wait_for_finish(agrpc::use_sender),
                                                            [&, ptr = std::move(ptr)](bool ok)
                                                            {
                                                                finish_ok = ok;
                                                            }));
        }
        else
        {
            finish_ok = true;
        }
        return rpc.get();
    };
    auto [status, response] = make_unary_request();
    CHECK_EQ(grpc::StatusCode::OK, status.error_code());
    CHECK_EQ(42, response.integer());
    test::sync_wait(test::scope_on_empty(scope));
    CHECK(finish_ok);
}

TEST_CASE_FIXTURE(ServerCallbackStdexecTest, "stdexec Client-streaming callback coroutine")
{
    exec::async_scope scope;
    service.client_streaming = [&](grpc::CallbackServerContext*, Response*) -> grpc::ServerReadReactor<Request>*
    {
        auto ptr = agrpc::make_reactor<agrpc::BasicServerReadReactor<Request, void>>();
        auto* r = ptr->get();
        test::scope_spawn_detached(scope,
                                   [](auto self, auto reactor) -> exec::task<void>
                                   {
                                       reactor->initiate_read(self->server_request);
                                       bool ok = co_await reactor->wait_for_read(agrpc::use_sender);
                                       CHECK(ok);
                                       CHECK_EQ(1, self->server_request.integer());
                                       reactor->initiate_read(self->server_request);
                                       ok = co_await reactor->wait_for_read(agrpc::use_sender);
                                       CHECK(ok);
                                       CHECK_EQ(2, self->server_request.integer());
                                       reactor->initiate_finish(grpc::Status::OK);
                                   }(this, std::move(ptr)));
        return r;
    };
    auto rpc = agrpc::make_reactor<agrpc::BasicClientWriteReactor<Request, void>>();
    test::set_default_deadline(rpc->context());
    rpc->start(&test::v1::Test::Stub::async::ClientStreaming, stub->async(), client_response);
    client_request.set_integer(1);
    rpc->initiate_write(client_request);
    CHECK(test::sync_wait(rpc->wait_for_write(agrpc::use_sender)));
    client_request.set_integer(2);
    rpc->initiate_write(client_request);
    CHECK(test::sync_wait(rpc->wait_for_write(agrpc::use_sender)));
    auto status = test::sync_wait(rpc->wait_for_finish(agrpc::use_sender));
    REQUIRE(status);
    CHECK_EQ(grpc::StatusCode::OK, status->error_code());
    test::sync_wait(test::scope_on_empty(scope));
}

TEST_CASE_FIXTURE(ServerCallbackStdexecTest, "stdexec Client-streaming callback coroutine cancel after write")
{
    exec::async_scope scope;
    service.client_streaming = [&](grpc::CallbackServerContext*, Response*) -> grpc::ServerReadReactor<Request>*
    {
        auto ptr = agrpc::make_reactor<agrpc::BasicServerReadReactor<Request, void>>();
        auto* r = ptr->get();
        test::scope_spawn_detached(scope,
                                   [](auto self, auto reactor) -> exec::task<void>
                                   {
                                       reactor->initiate_read(self->server_request);
                                       bool ok = co_await reactor->wait_for_read(agrpc::use_sender);
                                       self->server_done();
                                       CHECK(ok);
                                       CHECK_EQ(1, self->server_request.integer());
                                       reactor->initiate_read(self->server_request);
                                       ok = co_await reactor->wait_for_read(agrpc::use_sender);
                                       CHECK_FALSE(ok);
                                   }(this, std::move(ptr)));
        return r;
    };
    auto rpc = agrpc::make_reactor<agrpc::BasicClientWriteReactor<Request, void>>();
    test::set_default_deadline(rpc->context());
    rpc->start(&test::v1::Test::Stub::async::ClientStreaming, stub->async(), client_response);
    client_request.set_integer(1);
    rpc->initiate_write(client_request);
    CHECK(*test::sync_wait(rpc->wait_for_write(agrpc::use_sender)));
    wait_for_server_done();
    rpc->context().TryCancel();
    rpc->initiate_write(client_request);
    CHECK_FALSE(*test::sync_wait(rpc->wait_for_write(agrpc::use_sender)));
    auto status = test::sync_wait(rpc->wait_for_finish(agrpc::use_sender));
    test::sync_wait(rpc->wait_for_finish(agrpc::use_sender));
    CHECK_EQ(grpc::StatusCode::CANCELLED, status->error_code());
    test::sync_wait(test::scope_on_empty(scope));
}
