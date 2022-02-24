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

add_library(asio-grpc-coverage-options INTERFACE)

if(ASIO_GRPC_TEST_COVERAGE)
    target_compile_options(asio-grpc-coverage-options INTERFACE --coverage $<$<CXX_COMPILER_ID:GNU>:-fprofile-abs-path>)

    target_link_options(asio-grpc-coverage-options INTERFACE --coverage)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(ASIO_GRPC_GCOV_PROGRAM gcov)
    endif()
    if(NOT ASIO_GRPC_GCOV_PROGRAM)
        find_program(ASIO_GRPC_LLVM_COV_PROGRAM NAMES llvm-cov llvm-cov-10 llvm-cov-11 llvm-cov-12 llvm-cov-13
                                                      llvm-cov-14)
        set(_asio_grpc_gcov_command "${ASIO_GRPC_LLVM_COV_PROGRAM} gcov")
    else()
        set(_asio_grpc_gcov_command "${ASIO_GRPC_GCOV_PROGRAM}")
    endif()
    find_program(ASIO_GRPC_GCOVR_PROGRAM gcovr REQUIRED)
    add_custom_target(
        asio-grpc-test-coverage
        COMMAND "${ASIO_GRPC_GCOVR_PROGRAM}" --gcov-executable "${_asio_grpc_gcov_command}" --sonarqube --output
                "${ASIO_GRPC_COVERAGE_OUTPUT_FILE}" --root "${ASIO_GRPC_PROJECT_ROOT}"
        WORKING_DIRECTORY "${ASIO_GRPC_PROJECT_ROOT}"
        VERBATIM)
endif()
