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

function(run_process)
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY "${WORKING_DIRECTORY}"
        TIMEOUT 200
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output)

    if(NOT ${_result} EQUAL 0)
        message(FATAL_ERROR "Command \"${ARGN}\" failed with: ${_result}\n${_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORKING_DIRECTORY}/.")
file(MAKE_DIRECTORY "${WORKING_DIRECTORY}")

# configure asio-grpc
run_process(
    "${ASIO_GRPC_CMAKE_INSTALL_TEST_CMAKE_COMMAND}"
    "-B"
    "${WORKING_DIRECTORY}/build"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_INSTALL_PREFIX=${WORKING_DIRECTORY}/install"
    "-DASIO_GRPC_USE_BOOST_CONTAINER=${ASIO_GRPC_USE_BOOST_CONTAINER}"
    "${SOURCE_DIR}")

# install asio-grpc
run_process(
    "${ASIO_GRPC_CMAKE_INSTALL_TEST_CMAKE_COMMAND}"
    --build
    "${WORKING_DIRECTORY}/build"
    --config
    ${CMAKE_BUILD_TYPE}
    --target
    install)

file(COPY "${TEST_SOURCE_DIR}/." DESTINATION "${WORKING_DIRECTORY}")

# configure test project
run_process(
    "${ASIO_GRPC_CMAKE_INSTALL_TEST_CMAKE_COMMAND}"
    "-B"
    "${WORKING_DIRECTORY}/test-build"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
    "-DBoost_USE_STATIC_RUNTIME=${Boost_USE_STATIC_RUNTIME}"
    "-DCMAKE_PREFIX_PATH=${WORKING_DIRECTORY}/install"
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
    "-DVCPKG_OVERLAY_TRIPLETS=${VCPKG_OVERLAY_TRIPLETS}"
    "-DVCPKG_MANIFEST_MODE=${VCPKG_MANIFEST_MODE}"
    "-DVCPKG_MANIFEST_INSTALL=${VCPKG_MANIFEST_INSTALL}"
    "-DVCPKG_OVERLAY_PORTS=${VCPKG_OVERLAY_PORTS}"
    "-DVCPKG_INSTALL_OPTIONS=${VCPKG_INSTALL_OPTIONS}"
    "-DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
    # Use generator-expression to prevent multi-config generators from creating a subdirectory
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${WORKING_DIRECTORY}/test-install/\$<BOOL:on>"
    "${WORKING_DIRECTORY}")

# build test project
run_process("${ASIO_GRPC_CMAKE_INSTALL_TEST_CMAKE_COMMAND}" --build "${WORKING_DIRECTORY}/test-build" --config
            ${CMAKE_BUILD_TYPE})

# run test project
run_process("${WORKING_DIRECTORY}/test-install/1/asio-grpc-cmake-test${CMAKE_EXECUTABLE_SUFFIX}")
