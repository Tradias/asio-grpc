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

# asio-grpc
add_library(asio-grpc INTERFACE)
add_library(asio-grpc::asio-grpc ALIAS asio-grpc)

unset(ASIO_GRPC_HEADER_FILES)
list(
    APPEND
    ASIO_GRPC_HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/asioGrpc.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpcContext.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpcContext.ipp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpcExecutor.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/initiate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/rpcs.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/asioForward.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/attributes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/completionHandlerWithPayload.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcCompletionQueueEvent.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContext.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContextImplementation.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContextImplementation.ipp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContextInteraction.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcExecutorBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcExecutorOptions.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/initiate.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/operation.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/rpcs.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/typeErasedOperation.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/utility.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/agrpc/detail/memoryResource.hpp")
set_target_properties(asio-grpc PROPERTIES ASIO_GRPC_HEADER_FILES "${ASIO_GRPC_HEADER_FILES}")
set_target_properties(asio-grpc PROPERTIES ASIO_GRPC_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/asioGrpc.cpp")
unset(ASIO_GRPC_HEADER_FILES)

target_compile_features(asio-grpc INTERFACE cxx_std_17)

target_include_directories(
    asio-grpc INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>" $<INSTALL_INTERFACE:include>)

if(ASIO_GRPC_USE_BOOST_CONTAINER)
    set(ASIO_GRPC_MEMORY_RESOURCE_INDLUCES
        [[#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>]])
    set(ASIO_GRPC_MEMORY_RESOURCE_NAMESPACE_ALIAS "namespace pmr = boost::container::pmr;")
else()
    set(ASIO_GRPC_MEMORY_RESOURCE_INDLUCES "#include <memory_resource>")
    set(ASIO_GRPC_MEMORY_RESOURCE_NAMESPACE_ALIAS "namespace pmr = std::pmr;")
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memoryResource.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/agrpc/detail/memoryResource.hpp" @ONLY)
unset(ASIO_GRPC_MEMORY_RESOURCE_INDLUCES)
unset(ASIO_GRPC_MEMORY_RESOURCE_NAMESPACE_ALIAS)

# installation
if(ASIO_GRPC_INSTALL)
    include(GNUInstallDirs)

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake" ARCH_INDEPENDENT
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/asio-grpc")

    install(
        TARGETS asio-grpc
        EXPORT asio-grpc_EXPORT_TARGETS
        PUBLIC_HEADER)

    install(
        EXPORT asio-grpc_EXPORT_TARGETS
        NAMESPACE asio-grpc::
        FILE asio-grpcConfig.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/asio-grpc")

    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/agrpc"
        TYPE INCLUDE
        FILES_MATCHING
        REGEX "[.hpp|.ipp]$")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/agrpc/detail/memoryResource.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/agrpc/detail")
endif()
