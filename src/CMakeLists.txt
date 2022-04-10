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

# asio-grpc
function(asio_grpc_create_interface_target _asio_grpc_name)
    add_library(${_asio_grpc_name} INTERFACE)
    add_library(asio-grpc::${_asio_grpc_name} ALIAS ${_asio_grpc_name})

    target_compile_features(${_asio_grpc_name} INTERFACE cxx_std_17)

    target_include_directories(
        ${_asio_grpc_name}
        INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
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
endfunction()

asio_grpc_create_interface_target(asio-grpc)
target_compile_definitions(asio-grpc INTERFACE AGRPC_BOOST_ASIO)

asio_grpc_create_interface_target(asio-grpc-standalone-asio)
target_compile_definitions(asio-grpc-standalone-asio INTERFACE AGRPC_STANDALONE_ASIO)

asio_grpc_create_interface_target(asio-grpc-unifex)
target_compile_definitions(asio-grpc-unifex INTERFACE AGRPC_UNIFEX)

if(ASIO_GRPC_BUILD_TESTS)
    target_link_libraries(asio-grpc INTERFACE Boost::headers)
    target_link_libraries(asio-grpc-standalone-asio INTERFACE asio::asio)
    target_link_libraries(asio-grpc-unifex INTERFACE unifex::unifex)
endif()

# asio-grpc sources
if(ASIO_GRPC_BUILD_TESTS)
    add_library(asio-grpc-sources INTERFACE)

    target_sources(
        asio-grpc-sources
        INTERFACE # cmake-format: sort
                  "${CMAKE_CURRENT_BINARY_DIR}/generated/agrpc/detail/memoryResource.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/asioGrpc.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/bindAllocator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/defaultCompletionToken.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/allocate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/allocateOperation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/asioForward.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/associatedCompletionHandler.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/atomicIntrusiveQueue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/bindAllocator.ipp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/config.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/defaultCompletionToken.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/forward.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcCompletionQueueEvent.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContext.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContext.ipp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContextImplementation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcContextImplementation.ipp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcExecutorBase.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcExecutorOptions.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcInitiate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcInitiator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcSender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpcSubmit.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/intrusiveQueue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/intrusiveQueueHook.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memoryResourceAllocator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/oneShotAllocator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/operation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/receiver.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/receiver.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedlyRequest.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedlyRequestSender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/rpc.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/scheduleSender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/senderOf.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/typeErasedOperation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/unbind.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/useSender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/utility.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/wait.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/workTrackingCompletionHandler.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/getCompletionQueue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpcContext.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpcExecutor.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpcInitiate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/pollContext.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/repeatedlyRequest.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/repeatedlyRequestContext.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/rpc.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/useAwaitable.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/useSender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/wait.hpp")
endif()
