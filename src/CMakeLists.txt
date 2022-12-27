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
    add_library(${PROJECT_NAME}::${_asio_grpc_name} ALIAS ${_asio_grpc_name})

    target_compile_features(${_asio_grpc_name} INTERFACE cxx_std_17)

    target_include_directories(${_asio_grpc_name} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                            $<INSTALL_INTERFACE:include>)

    if(NOT ASIO_GRPC_PROJECT_IS_TOP_LEVEL OR ASIO_GRPC_BUILD_TESTS)
        if(ASIO_GRPC_USE_BOOST_CONTAINER)
            target_compile_definitions(${_asio_grpc_name} INTERFACE AGRPC_USE_BOOST_CONTAINER)
        elseif(ASIO_GRPC_USE_RECYCLING_ALLOCATOR)
            target_compile_definitions(${_asio_grpc_name} INTERFACE AGRPC_USE_RECYCLING_ALLOCATOR)
        endif()
    endif()

    if(ASIO_GRPC_INSTALL)
        if(ASIO_GRPC_USE_BOOST_CONTAINER)
            set(_asio_grpc_memory_resource_hpp "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memory_resource_boost_pmr.hpp")
        elseif(ASIO_GRPC_USE_RECYCLING_ALLOCATOR)
            set(_asio_grpc_memory_resource_hpp
                "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memory_resource_recycling_allocator.hpp")
        else()
            set(_asio_grpc_memory_resource_hpp "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memory_resource_std_pmr.hpp")
        endif()
        configure_file("${_asio_grpc_memory_resource_hpp}"
                       "${CMAKE_CURRENT_BINARY_DIR}/generated/agrpc/detail/memory_resource.hpp" COPYONLY)
    endif()
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

    if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
        target_link_libraries(asio-grpc-unifex INTERFACE unifex::unifex)
    endif()
endif()

# asio-grpc sources
if(ASIO_GRPC_BUILD_TESTS AND ASIO_GRPC_ENABLE_CHECK_HEADER_SYNTAX_TARGET)
    add_library(asio-grpc-sources INTERFACE)

    target_sources(
        asio-grpc-sources
        INTERFACE # cmake-format: sort
                  "${CMAKE_CURRENT_BINARY_DIR}/generated/agrpc/detail/memory_resource.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/alarm.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/asio_grpc.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/bind_allocator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/cancel_safe.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/default_completion_token.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/alarm.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/algorithm.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/allocate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/allocate_operation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/allocation_type.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/asio_association.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/asio_forward.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/associated_completion_handler.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/async_initiate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/atomic.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/atomic_intrusive_queue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/basic_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/buffer_allocator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/cancel_safe.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/completion_handler_receiver.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/conditional_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/config.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/coroutine_traits.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/default_completion_token.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/execution.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/execution_asio.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/execution_unifex.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/executor_with_default.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/forward.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/get_completion_queue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_completion_queue_event.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_context.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_context.ipp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_context_implementation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_context_implementation.ipp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_executor_base.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_executor_options.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_initiate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_initiator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/grpc_submit.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/health_check_repeatedly_request.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/high_level_client_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/initiate_sender_implementation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/intrusive_list.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/intrusive_list_hook.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/intrusive_queue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memory_resource.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/memory_resource_allocator.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/name.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/namespace_cpp20.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/no_op_stop_callback.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/notify_on_state_change.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/notify_when_done.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/operation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/operation_base.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/query_grpc_context.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/receiver.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/receiver_and_stop_callback.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedly_request.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedly_request_awaitable.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedly_request_base.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedly_request_context.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/repeatedly_request_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/rpc.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/rpc_client_context_base.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/rpc_context.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/schedule_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/sender_implementation.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/sender_of.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/server_write_reactor.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/serving_status.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/tagged_ptr.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/tuple.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/type_erased_completion_handler.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/unbind.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/use_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/utility.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/wait.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/detail/work_tracking_completion_handler.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/get_completion_queue.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpc_context.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpc_executor.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpc_initiate.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/grpc_stream.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/high_level_client.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/notify_on_state_change.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/notify_when_done.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/repeatedly_request.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/repeatedly_request_context.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/rpc.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/run.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/test.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/use_awaitable.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/use_sender.hpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/agrpc/wait.hpp")
endif()
