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

function(asio_grpc_add_coverage_flags _asio_grpc_target)
    target_compile_options(${_asio_grpc_target} PRIVATE --coverage)
    target_link_options(${_asio_grpc_target} PRIVATE --coverage)
endfunction()

function(asio_grpc_coverage_report_for_target _asio_grpc_target _asio_grpc_source)
    get_filename_component(_asio_grpc_source_name "${_asio_grpc_source}" NAME)
    find_program(ASIO_GRPC_GCOV_PROGRAM gcov)
    if(NOT ASIO_GRPC_GCOV_PROGRAM)
        find_program(ASIO_GRPC_LLVM_COV_PROGRAM NAMES llvm-cov llvm-cov-10 llvm-cov-11 llvm-cov-12 llvm-cov-13
                                                      llvm-cov-14)
        set(_asio_grpc_coverage_command "${ASIO_GRPC_LLVM_COV_PROGRAM}" gcov)
    else()
        set(_asio_grpc_coverage_command "${ASIO_GRPC_GCOV_PROGRAM}")
    endif()
    add_custom_target(
        ${_asio_grpc_target}-coverage
        DEPENDS ${_asio_grpc_target}
        COMMAND
            ${_asio_grpc_coverage_command} --relative-only --demangled-names --preserve-paths -o
            "$<TARGET_FILE_DIR:${_asio_grpc_target}>/CMakeFiles/${_asio_grpc_target}.dir/${_asio_grpc_source_name}.gcda"
            "${_asio_grpc_source}"
        WORKING_DIRECTORY "${ASIO_GRPC_COVERAGE_WORKING_DIR}"
        VERBATIM)
endfunction()
