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

#ifndef AGRPC_AGRPC_HEALTH_CHECK_SERVICE_HPP
#define AGRPC_AGRPC_HEALTH_CHECK_SERVICE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/health_check_repeatedly_request.hpp>
#include <agrpc/detail/serving_status.hpp>
#include <agrpc/grpc_context.hpp>
#include <grpcpp/ext/health_check_service_server_builder_option.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server_builder.h>

#include <map>
#include <string>

#ifdef __has_include
#if __has_include("grpc/health/v1/health.grpc.pb.h")
#include <grpc/health/v1/health.grpc.pb.h>
#endif
#endif

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief CompletionQueue implementation of grpc::HealthCheckServiceInterface
 *
 * @since 2.3.0
 */
class HealthCheckService final : public grpc::HealthCheckServiceInterface
{
  public:
    HealthCheckService(grpc::ServerBuilder& builder);

    /// Set or change the serving status of the given \a service_name.
    void SetServingStatus(const std::string& service_name, bool serving) override;

    /// Apply to all registered service names.
    void SetServingStatus(bool serving) override;

    /// Set all registered service names to not serving and prevent future
    /// state changes.
    void Shutdown();

    friend void start_health_check_service(grpc::HealthCheckServiceInterface* service,
                                           agrpc::GrpcContext& grpc_context);

  private:
    friend detail::HealthCheckRepeatedlyRequestWatch;
    friend detail::HealthCheckWatcher;
    friend detail::HealthCheckRepeatedlyRequestCheck;
    friend detail::HealthCheckChecker;

    detail::ServingStatus get_serving_status(const std::string& service_name) const;

    agrpc::GrpcContext* grpc_context;
    grpc::health::v1::Health::AsyncService service;
    std::map<std::string, detail::HealthCheckServiceData> services_map;
    detail::HealthCheckRepeatedlyRequestWatch repeatedly_request_watch;
    detail::HealthCheckRepeatedlyRequestCheck repeatedly_request_check;
    bool is_shutdown{false};
};

/**
 * @brief CompletionQueue implementation of grpc::HealthCheckServiceInterface
 *
 * @since 2.3.0
 */
grpc::ServerBuilder& add_health_check_service(grpc::ServerBuilder& builder);

void start_health_check_service(grpc::HealthCheckServiceInterface* service, agrpc::GrpcContext& grpc_context);

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HEALTH_CHECK_SERVICE_HPP

#include <agrpc/detail/health_check_service.hpp>
