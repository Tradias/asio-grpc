# Copyright 2022 Dennis Hezel
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

install(TARGETS asio-grpc asio-grpc-standalone-asio asio-grpc-unifex EXPORT ${PROJECT_NAME}Targets)

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}")

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/agrpc"
    TYPE INCLUDE
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.ipp")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/src/generated/agrpc/detail/memoryResource.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/agrpc/detail")
