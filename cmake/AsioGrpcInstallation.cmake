# Copyright 2026 Dennis Hezel
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/asio-grpcConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}Config.cmake" @ONLY)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}ConfigVersion.cmake" ARCH_INDEPENDENT
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}ConfigVersion.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}Config.cmake"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AsioGrpcProtobufGenerator.cmake"
    DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}")

install(
    FILES "${ASIO_GRPC_PROJECT_ROOT}/asio-grpc.natvis"
    DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}"
    RENAME "${PROJECT_NAME}.natvis")

install(TARGETS asio-grpc asio-grpc-standalone-asio asio-grpc-unifex asio-grpc-stdexec EXPORT ${PROJECT_NAME}Targets)

# Install C++20 module targets when they were created (CMake 3.28+).
# FILE_SET CXX_MODULES causes CMake to install the .cppm sources alongside the
# headers so that consumers' build systems can compile the BMI themselves
# (pre-compiled BMIs are compiler-version-specific and cannot be distributed).
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    install(
        TARGETS asio-grpc-module
                asio-grpc-standalone-asio-module
                asio-grpc-unifex-module
                asio-grpc-stdexec-module
                asio-grpc-health-check-module
                asio-grpc-standalone-asio-health-check-module
                asio-grpc-unifex-health-check-module
                asio-grpc-stdexec-health-check-module
        EXPORT ${PROJECT_NAME}Targets
        FILE_SET CXX_MODULES DESTINATION include)
endif()

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}")

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/agrpc"
    TYPE INCLUDE
    FILES_MATCHING
    PATTERN "*.hpp")

# Install .cppm sources for module-aware consumers (CMake 3.28+).
# These are installed separately from FILE_SET so that the paths are
# predictable for packaging tools (vcpkg, Conan) that pre-stage sources.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/asio_grpc.cppm"
              "${CMAKE_CURRENT_SOURCE_DIR}/src/asio_grpc_health_check.cppm"
        TYPE INCLUDE)
endif()
