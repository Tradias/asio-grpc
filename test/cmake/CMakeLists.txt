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

if("${CMAKE_GENERATOR}" STREQUAL "Ninja Multi-Config")
    if(WIN32)
        set(ASIO_GRPC_CMAKE_INSTALL_TEST_GENERATOR "NMake Makefiles")
    else()
        set(ASIO_GRPC_CMAKE_INSTALL_TEST_GENERATOR "Unix Makefiles")
    endif()
else()
    set(ASIO_GRPC_CMAKE_INSTALL_TEST_GENERATOR "${CMAKE_GENERATOR}")
endif()

function(add_cmake_test _name)
    set(ASIO_GRPC_CMAKE_INSTALL_TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${_name}")

    configure_file("${ASIO_GRPC_CMAKE_INSTALL_TEST_SOURCE_DIR}/CMakeLists.txt.in"
                   "${ASIO_GRPC_CMAKE_INSTALL_TEST_SOURCE_DIR}/CMakeLists.txt" @ONLY)

    add_test(
        NAME asio-grpc-cmake-test-${_name}
        COMMAND
            "${ASIO_GRPC_CMAKE_INSTALL_TEST_CTEST_COMMAND}" "--build-and-test"
            "${ASIO_GRPC_CMAKE_INSTALL_TEST_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/${_name}" "--build-generator"
            "${ASIO_GRPC_CMAKE_INSTALL_TEST_GENERATOR}" "--build-project" "asio-grpc-cmake-test-${_name}"
            "--test-command" "${CMAKE_CURRENT_BINARY_DIR}/${_name}/test-install/main${CMAKE_EXECUTABLE_SUFFIX}")
endfunction()

add_cmake_test(superbuild)
add_cmake_test(subdirectory)
