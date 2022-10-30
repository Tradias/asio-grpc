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

struct HealthCheckServiceTest : test::GrpcContextTest
{
    using CheckRPC = agrpc::RPC<&grpc_health::Health::Stub::PrepareAsyncCheck>;
    using WatchRPC = agrpc::RPC<&grpc_health::Health::Stub::PrepareAsyncWatch>;

    grpc::ServerContext server_context;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<grpc_health::Health::Stub> stub;
    std::optional<test::ServerShutdownInitiator> server_shutdown;
    grpc::ClientContext client_context;
    grpc_health::HealthCheckRequest request;
    grpc_health::HealthCheckResponse response;

    HealthCheckServiceTest()
    {
        agrpc::add_health_check_service(builder);
        const auto port = test::get_free_port();
        const auto address = std::string{"0.0.0.0:"} + std::to_string(port);
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        server = builder.BuildAndStart();
        agrpc::start_health_check_service(server->GetHealthCheckService(), grpc_context);
        channel =
            grpc::CreateChannel(std::string{"127.0.0.1:"} + std::to_string(port), grpc::InsecureChannelCredentials());
        stub = grpc_health::Health::NewStub(channel);
        client_context.set_deadline(test::five_seconds_from_now());
        server_shutdown.emplace(*server);
    }

    ~HealthCheckServiceTest() { stub.reset(); }

    void shutdown() { server_shutdown->initiate(); }
};

TEST_CASE_FIXTURE(HealthCheckServiceTest, "health_check_service: check default service")
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
            shutdown();
        });
}

TEST_CASE_FIXTURE(HealthCheckServiceTest, "health_check_service: watch default service and change serving status")
{
    test::spawn_and_run(grpc_context,
                        [&](const asio::yield_context& yield)
                        {
                            auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                            CHECK(rpc.ok());
                            CHECK(rpc.read(response, yield));
                            CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVING, response.status());
                            server->GetHealthCheckService()->SetServingStatus(false);
                            CHECK(rpc.read(response, yield));
                            CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_NOT_SERVING, response.status());
                            shutdown();
                        });
}

TEST_CASE_FIXTURE(HealthCheckServiceTest, "health_check_service: watch non-existent service")
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
                            CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVICE_UNKNOWN, response.status());
                            if (add_service)
                            {
                                server->GetHealthCheckService()->SetServingStatus("non-existent", true);
                                CHECK(rpc.read(response, yield));
                                CHECK_EQ(grpc_health::HealthCheckResponse_ServingStatus_SERVING, response.status());
                            }
                            shutdown();
                        });
}

TEST_CASE_FIXTURE(HealthCheckServiceTest, "health_check_service: watch default service and shutdown HealthCheckService")
{
    test::spawn_and_run(grpc_context,
                        [&](const asio::yield_context& yield)
                        {
                            auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                            CHECK(rpc.read(response, yield));
                            server->GetHealthCheckService()->Shutdown();
                            CHECK_FALSE(rpc.read(response, yield));
                            CHECK_EQ("not writing due to shutdown", rpc.status().error_message());
                            shutdown();
                        });
}

TEST_CASE_FIXTURE(HealthCheckServiceTest, "health_check_service: watch default service and cancel")
{
    test::spawn_and_run(grpc_context,
                        [&](const asio::yield_context& yield)
                        {
                            auto rpc = WatchRPC::request(grpc_context, *stub, client_context, request, yield);
                            CHECK(rpc.read(response, yield));
                            client_context.TryCancel();
                            CHECK_FALSE(rpc.read(response, yield));
                            CHECK_EQ(grpc::StatusCode::CANCELLED, rpc.status_code());
                            shutdown();
                        });
}
