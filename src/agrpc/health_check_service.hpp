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
 * @brief CompletionQueue-based implementation of grpc::HealthCheckServiceInterface
 *
 * This class is a drop-in replacement for the `grpc::DefaultHealthCheckService`. It should be added to a
 * `grpc::ServerBuilder` using `agrpc::add_health_check_service`.
 *
 * **Motivation**: `grpc::DefaultHealthCheckService` is implemented in terms of gRPC's generic callback API. Mixing
 * callback services and CompletionQueue-based services in one `grpc::Server` leads to significant performance
 * degradation.
 *
 * @note In order to use this class you must compile and link with
 * [health.proto](https://github.com/grpc/grpc/blob/v1.50.1/src/proto/grpc/health/v1/health.proto). If your compiler
 * does not support `__has_include` then you must also include `health.grpc.pb.h` before including
 * `agrpc/health_check_service.hpp`.
 *
 * @since 2.3.0
 */
class HealthCheckService final : public grpc::HealthCheckServiceInterface
{
  public:
    explicit HealthCheckService(grpc::ServerBuilder& builder);

    /**
     * @brief Set or change the serving status of the given @a service_name
     *
     * Thread-safe
     */
    void SetServingStatus(const std::string& service_name, bool serving) override;

    /**
     * @brief Apply a serving status to all registered service names
     *
     * Thread-safe
     */
    void SetServingStatus(bool serving) override;

    /**
     * @brief Set all registered service names to not serving and prevent future state changes.
     *
     * Thread-safe
     */
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
 * @brief Add a HealthCheckService to a `grpc::Server`
 *
 * The service must be started using `agrpc::start_health_check_service` after `builder.BuildAndStart()` has been
 * called.
 *
 * Example:
 *
 * @snippet server.cpp add-health-check-service
 *
 * @since 2.3.0
 *
 * @relates HealthCheckService
 */
grpc::ServerBuilder& add_health_check_service(grpc::ServerBuilder& builder);

/**
 * @brief Start a previously added HealthCheckService
 *
 * The service must have been added using `agrpc::add_health_check_service()`. May not be called concurrently with
 * `GrpcContext::run/poll`.
 *
 * @since 2.3.0
 *
 * @relates HealthCheckService
 */
void start_health_check_service(grpc::HealthCheckServiceInterface* service, agrpc::GrpcContext& grpc_context);

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HEALTH_CHECK_SERVICE_HPP

#include <agrpc/detail/health_check_service.hpp>
