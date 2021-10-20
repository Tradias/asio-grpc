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

function(run_process)
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY "${PWD}"
        TIMEOUT 200
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output)

    if(NOT ${_result} EQUAL 0)
        message(FATAL_ERROR "Command \"${ARGN}\" failed with: ${_result}\n${_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${PWD}/.")
file(MAKE_DIRECTORY "${PWD}")

# configure asio-grpc
run_process(
    "${CMAKE_COMMAND}"
    "-B"
    "${PWD}/build"
    "-G${CMAKE_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_INSTALL_PREFIX=${PWD}/install"
    "-DASIO_GRPC_USE_BOOST_CONTAINER=${ASIO_GRPC_USE_BOOST_CONTAINER}"
    "${SOURCE_DIR}")

# install asio-grpc
run_process(
    "${CMAKE_COMMAND}"
    --build
    "${PWD}/build"
    --config
    ${CMAKE_BUILD_TYPE}
    --target
    install)

file(COPY "${TEST_SOURCE_DIR}/." DESTINATION "${PWD}")

# configure test project
run_process(
    "${CMAKE_COMMAND}"
    "-B"
    "${PWD}/test-build"
    "-G${CMAKE_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
    "-DBoost_USE_STATIC_RUNTIME=${Boost_USE_STATIC_RUNTIME}"
    "-DCMAKE_PREFIX_PATH=${PWD}/install"
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
    "-DVCPKG_MANIFEST_MODE=${VCPKG_MANIFEST_MODE}"
    "-DASIO_GRPC_TEST_PROTOS=${ASIO_GRPC_TEST_PROTOS}"
    # Use generator-expression to prevent multi-config generators from creating a subdirectory
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${PWD}/test-install/\$<BOOL:on>"
    "${PWD}")

# build test project
run_process("${CMAKE_COMMAND}" --build "${PWD}/test-build" --config ${CMAKE_BUILD_TYPE})

# run test project
run_process("${PWD}/test-install/1/asio-grpc-cmake-test${CMAKE_EXECUTABLE_SUFFIX}")
