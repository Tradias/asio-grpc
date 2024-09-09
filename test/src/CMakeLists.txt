# Copyright 2024 Dennis Hezel
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

# tests
function(asio_grpc_add_test _asio_grpc_name _asio_grpc_type _asio_grpc_cxx_standard)
    add_executable(${_asio_grpc_name})

    target_sources(${_asio_grpc_name} PRIVATE ${ARGN})

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(
        ${_asio_grpc_name}
        PRIVATE "asio-grpc-test-main${_asio_grpc_cxx_standard}" "asio-grpc-test-protos${_asio_grpc_cxx_standard}"
                "asio-grpc-test-util-${_asio_grpc_type}${_asio_grpc_cxx_standard}")

    target_precompile_headers(${_asio_grpc_name} REUSE_FROM
                              "asio-grpc-test-util-${_asio_grpc_type}${_asio_grpc_cxx_standard}")

    if(ASIO_GRPC_DISCOVER_TESTS)
        doctest_discover_tests(${_asio_grpc_name})
    endif()
endfunction()

set(ASIO_GRPC_CPP17_TEST_SOURCE_FILES
    "test_alarm_17.cpp"
    "test_asio_grpc_17.cpp"
    "test_bind_allocator_17.cpp"
    "test_grpc_context_17.cpp"
    "test_run_17.cpp"
    "test_test_17.cpp"
    "test_health_check_service_17.cpp"
    "test_client_rpc_17.cpp"
    "test_server_rpc_17.cpp"
    "test_waiter_17.cpp")
set(ASIO_GRPC_CPP20_TEST_SOURCE_FILES "test_alarm_20.cpp" "test_bind_allocator_20.cpp" "test_grpc_context_20.cpp"
                                      "test_server_rpc_20.cpp")

if(NOT CMAKE_UNITY_BUILD)
    set_source_files_properties(
        "test_client_rpc_17.cpp" "test_server_rpc_17.cpp" "test_server_rpc_20.cpp" "test_unifex_20.cpp"
        "test_execution.cpp" PROPERTIES COMPILE_OPTIONS "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/bigobj>")
endif()

asio_grpc_add_test(asio-grpc-test-boost-cpp17 "boost-asio" "17" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES})
asio_grpc_add_test(asio-grpc-test-cpp17 "standalone-asio" "17" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES})

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_test(asio-grpc-test-boost-cpp20 "boost-asio" "20" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES}
                       ${ASIO_GRPC_CPP20_TEST_SOURCE_FILES})
    asio_grpc_add_test(asio-grpc-test-cpp20 "standalone-asio" "20" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES}
                       ${ASIO_GRPC_CPP20_TEST_SOURCE_FILES})
    asio_grpc_add_test(asio-grpc-test-unifex-cpp20 "unifex" "20" "test_unifex_20.cpp" "test_execution.cpp")
    asio_grpc_add_test(
        asio-grpc-test-boost-unifex-cpp20
        "boost-asio-unifex"
        "20"
        ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES}
        ${ASIO_GRPC_CPP20_TEST_SOURCE_FILES}
        "test_unifex_20.cpp"
        "test_execution.cpp")
endif()

if(ASIO_GRPC_ENABLE_STDEXEC_TESTS)
    asio_grpc_add_test(asio-grpc-test-stdexec "stdexec" "20" "test_execution.cpp")

    if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
        asio_grpc_add_test(
            asio-grpc-test-asio-stdexec-cpp20 "standalone-asio-stdexec" "20" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES}
            ${ASIO_GRPC_CPP20_TEST_SOURCE_FILES} "test_execution.cpp")
    endif()
endif()

unset(ASIO_GRPC_CPP17_TEST_SOURCE_FILES)
unset(ASIO_GRPC_CPP20_TEST_SOURCE_FILES)
