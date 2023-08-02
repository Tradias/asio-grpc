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
#include "utils/asio_utils.hpp"
#include "utils/client_rpc.hpp"
#include "utils/doctest.hpp"
#include "utils/rpc.hpp"
#include "utils/server_rpc.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/client_rpc.hpp>
#include <agrpc/server_rpc.hpp>

#include <cstddef>

template <auto RequestRPC, class Service, class RequestHandler>
void request_loop(agrpc::GrpcContext& grpc_context, Service& service, RequestHandler request_handler,
                  const asio::yield_context& yield)
{
    using RPC = agrpc::ServerRPC<RequestRPC>;
    RPC rpc{grpc_context.get_executor()};
    typename RPC::Request request;
    if (!rpc.start(service, request, yield))
    {
        return;
    }
    test::typed_spawn(grpc_context,
                      [&](const asio::yield_context& yield)
                      {
                          request_loop<RequestRPC>(grpc_context, service, request_handler, yield);
                      });
    std::exception_ptr eptr;
    AGRPC_TRY { request_handler(rpc, request, yield); }
    AGRPC_CATCH(...) { eptr = std::current_exception(); }
    if (!rpc.is_finished())
    {
        rpc.cancel();
    }
    if (rpc.is_running())
    {
        rpc.done(yield);
    }
    if (eptr)
    {
        std::rethrow_exception(eptr);
    }
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ServerStreamingClientRPC>, "ServerRPC server streaming success")
{
    auto request_handler =
        [&](test::ServerStreamingServerRPC& rpc, test::msg::Request& client_request, const asio::yield_context& yield)
    {
        CHECK_EQ(42, client_request.integer());
        test_server.response.set_integer(21);
        CHECK(rpc.write(test_server.response, yield));
        CHECK(rpc.finish(grpc::Status::OK, yield));
    };
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            request_loop<&test::v1::Test::AsyncService::RequestServerStreaming>(grpc_context, service, request_handler,
                                                                                yield);
        },
        [&](const asio::yield_context& yield)
        {
            test::ServerStreamingClientRPC rpc{grpc_context};
            request.set_integer(42);
            start_rpc(rpc, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(21, response.integer());
            CHECK(rpc.finish(yield).ok());
            server_shutdown.initiate();
        });
}

TEST_CASE_FIXTURE(test::ClientRPCTest<test::ServerStreamingClientRPC>, "ServerRPC server streaming no finish")
{
    auto request_handler = [&](test::ServerStreamingServerRPC& rpc, auto&, const asio::yield_context& yield)
    {
        CHECK(rpc.write(test_server.response, yield));
    };
    spawn_and_run(
        [&](const asio::yield_context& yield)
        {
            request_loop<&test::v1::Test::AsyncService::RequestServerStreaming>(grpc_context, service, request_handler,
                                                                                yield);
        },
        [&](const asio::yield_context& yield)
        {
            test::ServerStreamingClientRPC rpc{grpc_context};
            start_rpc(rpc, yield);
            CHECK(rpc.read(response, yield));
            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.finish(yield).error_code());
            server_shutdown.initiate();
        });
}