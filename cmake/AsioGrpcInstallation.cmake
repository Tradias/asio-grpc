# Copyright 2021 Dennis Hezel
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

include(GNUInstallDirs)

set(ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/asio-grpc")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/asio-grpcConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfig.cmake"
    INSTALL_DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}"
    NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake" ARCH_INDEPENDENT
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AsioGrpcProtobufGenerator.cmake"
    DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}")

install(TARGETS asio-grpc EXPORT asio-grpc_EXPORT_TARGETS)

install(
    EXPORT asio-grpc_EXPORT_TARGETS
    NAMESPACE asio-grpc::
    FILE asio-grpcTargets.cmake
    DESTINATION "${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}")

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/agrpc"
    TYPE INCLUDE
    FILES_MATCHING
    REGEX "[.hpp|.ipp]$")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/src/generated/agrpc/detail/memoryResource.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/agrpc/detail")

unset(ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR)
