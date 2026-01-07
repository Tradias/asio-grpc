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

# asio-grpc
function(asio_grpc_create_interface_target _asio_grpc_name)
    add_library(${_asio_grpc_name} INTERFACE)
    add_library(${PROJECT_NAME}::${_asio_grpc_name} ALIAS ${_asio_grpc_name})

    target_compile_features(${_asio_grpc_name} INTERFACE cxx_std_17)

    target_include_directories(${_asio_grpc_name} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                                                            $<INSTALL_INTERFACE:include>)
endfunction()

asio_grpc_create_interface_target(asio-grpc)
target_compile_definitions(asio-grpc INTERFACE AGRPC_BOOST_ASIO)

asio_grpc_create_interface_target(asio-grpc-standalone-asio)
target_compile_definitions(asio-grpc-standalone-asio INTERFACE AGRPC_STANDALONE_ASIO)

asio_grpc_create_interface_target(asio-grpc-unifex)
target_compile_definitions(asio-grpc-unifex INTERFACE AGRPC_UNIFEX)

asio_grpc_create_interface_target(asio-grpc-stdexec)
target_compile_definitions(asio-grpc-stdexec INTERFACE AGRPC_STDEXEC)
target_compile_features(asio-grpc-stdexec INTERFACE cxx_std_20)

if(ASIO_GRPC_BUILD_EXAMPLES)
    target_link_libraries(asio-grpc INTERFACE Boost::headers)

    if(TARGET asio::asio)
        target_link_libraries(asio-grpc-standalone-asio INTERFACE asio::asio)
    endif()

    if(TARGET unifex::unifex)
        target_link_libraries(asio-grpc-unifex INTERFACE unifex::unifex)
    endif()

    if(TARGET STDEXEC::stdexec)
        target_link_libraries(asio-grpc-stdexec INTERFACE STDEXEC::stdexec)
    endif()
endif()

# asio-grpc sources
if(ASIO_GRPC_BUILD_TESTS AND ASIO_GRPC_ENABLE_CHECK_HEADER_SYNTAX_TARGET)
    add_library(asio-grpc-sources INTERFACE)

    file(GLOB_RECURSE _asio_grpc_sources "agrpc/*.hpp")

    list(
        TRANSFORM _asio_grpc_sources
        REPLACE
            "(execution_stdexec.hpp|association_execution.hpp|health_check_service.hpp|grpc_context_definition.hpp)$"
            "")

    if(NOT ASIO_GRPC_TEST_CALLBACK_API)
        list(TRANSFORM _asio_grpc_sources
             REPLACE "(reactor_ptr.hpp|server_callback.hpp|server_callback_coroutine.hpp|client_callback.hpp)$" "")
    elseif(NOT ASIO_GRPC_TEST_CALLBACK_API_CPP20)
        list(TRANSFORM _asio_grpc_sources REPLACE "(server_callback_coroutine.hpp)$" "")
    endif()

    target_sources(asio-grpc-sources INTERFACE ${_asio_grpc_sources})
endif()
