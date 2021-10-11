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

function(run_process _working_dir)
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY "${_working_dir}"
        TIMEOUT 120
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output)

    if(NOT ${_result} EQUAL 0)
        message(FATAL_ERROR "Command \"${ARGN}\" failed with: ${_result}\n${_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${PWD}/.")
file(MAKE_DIRECTORY "${PWD}/build")

# configure asio-grpc
run_process("${PWD}/build" "${CMAKE_COMMAND}" "-DCMAKE_INSTALL_PREFIX=${PWD}/install" "${SOURCE_DIR}")

# install asio-grpc
run_process("${PWD}/build" "${CMAKE_COMMAND}" --build . --target install)

file(MAKE_DIRECTORY "${PWD}/test-build")

# configure test project
run_process(
    "${PWD}/test-build"
    "${CMAKE_COMMAND}"
    "-G${CMAKE_GENERATOR}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
    "-DCMAKE_PREFIX_PATH=${PWD}/install\;${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}"
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
    "-DCMAKE_INSTALL_PREFIX=${PWD}/test-install"
    "-DProtobuf_PROTOC_EXECUTABLE=${Protobuf_PROTOC_EXECUTABLE}"
    "-DASIO_GRPC_TEST_PROTOS=${ASIO_GRPC_TEST_PROTOS}"
    "${TEST_SOURCE_DIR}")

# build test project
run_process("${PWD}/test-build" "${CMAKE_COMMAND}" --build . --target install)

# run test project
run_process("${PWD}/test-install" "${PWD}/test-install/bin/asio-grpc-cmake-test${CMAKE_EXECUTABLE_SUFFIX}")
