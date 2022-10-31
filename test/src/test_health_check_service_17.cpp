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

#include "grpc/health/v1/health.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/free_port.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/server_shutdown_initiator.hpp"
#include "utils/time.hpp"

#include <agrpc/health_check_service.hpp>
#include <agrpc/high_level_client.hpp>
#include <grpcpp/create_channel.h>

namespace grpc_health = grpc::health::v1;

template <bool UseAgrpc>
struct HealthCheckServiceTest : test::GrpcContextTest
{
    using CheckRPC = agrpc::RPC<&grpc_health::Health::Stub::PrepareAsyncCheck>;
    using WatchRPC = agrpc::RPC<&grpc_health::Health::Stub::PrepareAsyncWatch>;

    grpc::ServerContext server_context;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<grpc_health::Health::Stub> stub;
    grpc::ClientContext client_context;
    grpc_health::HealthCheckRequest request;
    grpc_health::HealthCheckResponse response;

    HealthCheckServiceTest()
    {
        if constexpr (UseAgrpc)
        {
            agrpc::add_health_check_service(builder);
        }
        else
        {
            grpc::EnableDefaultHealthCheckService(true);
        }
        const auto port = test::get_free_port();
        const auto address = std::string{"0.0.0.0:"} + std::to_string(port);
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        server = builder.BuildAndStart();
        if constexpr (UseAgrpc)
        {
            agrpc::start_health_check_service(server->GetHealthCheckService(), grpc_context);
        }
        channel =
            grpc::CreateChannel(std::string{"127.0.0.1:"} + std::to_string(port), grpc::InsecureChannelCredentials());
        stub = grpc_health::Health::NewStub(channel);
        client_context.set_deadline(test::five_seconds_from_now());
    }

    ~HealthCheckServiceTest()
    {
        stub.reset();
        server->Shutdown();
    }

    void test_check_default_service()
    {
        test::spawn_and_run(
            grpc_context,
            [&](const asio::yield_context& yield)
            {
                CHECK(CheckRPC::request(grpc_context, *stub, client_context, request, response, yield).ok());
                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVING, response.status());
                server->GetHealthCheckService()->SetServingStatus(false);
                grpc::ClientContext client_context2;
                CHECK(CheckRPC::request(grpc_context, *stub, client_context2, request, response, yield).ok());
                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_NOT_SERVING, response.status());
            });
    }

    void test_check_non_existent_service()
    {
        test::spawn_and_run(grpc_context,
                            [&](const asio::yield_context& yield)
                            {
                                request.set_service("non-existent");
                                const auto status =
                                    CheckRPC::request(grpc_context, *stub, client_context, request, response, yield);
                                CHECK_EQ(grpc::StatusCode::NOT_FOUND, status.error_code());
                                CHECK_EQ("service name unknown", status.error_message());
                            });
    }

    void test_watch_default_service_and_change_serving_status()
    {
        test::spawn_and_run(grpc_context,
                            [&](const asio::yield_context& yield)
                            {
                                auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                                CHECK(rpc.ok());
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVING, response.status());
                                server->GetHealthCheckService()->SetServingStatus(false);
                                server->GetHealthCheckService()->SetServingStatus(false);
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_NOT_SERVING, response.status());
                                server->GetHealthCheckService()->SetServingStatus(true);
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_NOT_SERVING, response.status());
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVING, response.status());
                            });
    }

    void test_watch_non_existent_service()
    {
        bool add_service{false};
        SUBCASE("always non-existent") {}
        SUBCASE("added later") { add_service = true; }
        test::spawn_and_run(grpc_context,
                            [&](const asio::yield_context& yield)
                            {
                                request.set_service("non-existent");
                                auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                                CHECK(rpc.ok());
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVICE_UNKNOWN,
                                         response.status());
                                if (add_service)
                                {
                                    server->GetHealthCheckService()->SetServingStatus("non-existent", true);
                                    CHECK(rpc.read(response, yield));
                                    CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVING, response.status());
                                }
                            });
    }

    // Older versions of gRPC do not have `HealthCheckServiceInterface::Shutdown()`
    template <class T, class = decltype(std::declval<T&>().Shutdown())>
    void test_watch_and_shutdown_health_check_service(T* health_check_service)
    {
        test::spawn_and_run(grpc_context,
                            [&](const asio::yield_context& yield)
                            {
                                auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                                CHECK(rpc.read(response, yield));
                                health_check_service->Shutdown();
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_NOT_SERVING, response.status());
                            });
    }

    template <class... T>
    void test_watch_and_shutdown_health_check_service(T&&...)
    {
    }

    void test_watch_and_client_cancel()
    {
        test::spawn_and_run(grpc_context,
                            [&](const asio::yield_context& yield)
                            {
                                auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                                CHECK(rpc.read(response, yield));
                                client_context.TryCancel();
                                CHECK_FALSE(rpc.read(response, yield));
                                CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
                            });
    }
};

using HealthCheckServiceAgrpcTest = HealthCheckServiceTest<true>;
using HealthCheckServiceGrpcTest = HealthCheckServiceTest<false>;

TYPE_TO_STRING(HealthCheckServiceAgrpcTest);
TYPE_TO_STRING(HealthCheckServiceGrpcTest);

TEST_CASE_TEMPLATE("health_check_service: check default service", T, HealthCheckServiceAgrpcTest,
                   HealthCheckServiceGrpcTest)
{
    T{}.test_check_default_service();
}

TEST_CASE_TEMPLATE("health_check_service: check non-existent service", T, HealthCheckServiceAgrpcTest,
                   HealthCheckServiceGrpcTest)
{
    T{}.test_check_non_existent_service();
}

TEST_CASE_TEMPLATE("health_check_service: watch default service and change serving status", T,
                   HealthCheckServiceAgrpcTest, HealthCheckServiceGrpcTest)
{
    T{}.test_watch_default_service_and_change_serving_status();
}

TEST_CASE_TEMPLATE("health_check_service: watch non-existent service", T, HealthCheckServiceAgrpcTest,
                   HealthCheckServiceGrpcTest)
{
    T{}.test_watch_non_existent_service();
}

TEST_CASE_TEMPLATE("health_check_service: watch default service and shutdown HealthCheckService", T,
                   HealthCheckServiceAgrpcTest, HealthCheckServiceGrpcTest)
{
    T test{};
    test.test_watch_and_shutdown_health_check_service(test.server->GetHealthCheckService());
}

TEST_CASE_TEMPLATE("health_check_service: watch default service and cancel", T, HealthCheckServiceAgrpcTest,
                   HealthCheckServiceGrpcTest)
{
    T{}.test_watch_and_client_cancel();
}
