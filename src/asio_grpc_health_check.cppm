// Copyright 2026 Dennis Hezel
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

// C++20 module interface unit for the asio-grpc health check service.
//
// Kept as a separate module from asio_grpc because health_check_service.hpp
// pulls in the gRPC health-check proto types (grpc::health::v1::Health) which
// not all users require.
//
// The backend is selected at CMake configure time via compile definitions
// (AGRPC_BOOST_ASIO / AGRPC_STANDALONE_ASIO / AGRPC_UNIFEX / AGRPC_STDEXEC),
// exactly as for the existing INTERFACE targets.
//
// Usage (CMake):
//   target_link_libraries(my_target PRIVATE asio-grpc::asio-grpc-health-check-module)
//
// Usage (C++):
//   import asio_grpc;
//   import asio_grpc.health_check;

module;

// ── Global module fragment ────────────────────────────────────────────────────
// Include the full convenience header so that all agrpc types used in
// HealthCheckService's interface (GrpcContext, etc.) are available, and
// include the health check header on top.

#include <agrpc/asio_grpc.hpp>
#include <agrpc/health_check_service.hpp>

// ── Named module interface ────────────────────────────────────────────────────
export module asio_grpc.health_check;

// Re-export the three public names exposed by health_check_service.hpp.

export using agrpc::HealthCheckService;
export using agrpc::add_health_check_service;
export using agrpc::start_health_check_service;
