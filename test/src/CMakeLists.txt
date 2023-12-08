# Copyright 2023 Dennis Hezel
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

    target_link_libraries(${_asio_grpc_name} PRIVATE "asio-grpc-test-main${_asio_grpc_cxx_standard}"
                                                     "asio-grpc-test-protos${_asio_grpc_cxx_standard}")
    if(${_asio_grpc_type} STREQUAL "BOOST_ASIO")
        target_link_libraries(${_asio_grpc_name} PRIVATE "asio-grpc-test-util-boost-asio${_asio_grpc_cxx_standard}")

        target_precompile_headers(${_asio_grpc_name} REUSE_FROM
                                  "asio-grpc-test-util-boost-asio${_asio_grpc_cxx_standard}")
    elseif(${_asio_grpc_type} STREQUAL "STANDALONE_ASIO")
        target_link_libraries(${_asio_grpc_name}
                              PRIVATE "asio-grpc-test-util-standalone-asio${_asio_grpc_cxx_standard}")

        target_precompile_headers(${_asio_grpc_name} REUSE_FROM
                                  "asio-grpc-test-util-standalone-asio${_asio_grpc_cxx_standard}")
    elseif(${_asio_grpc_type} STREQUAL "UNIFEX")
        target_link_libraries(${_asio_grpc_name} PRIVATE "asio-grpc-test-util-unifex${_asio_grpc_cxx_standard}")

        target_precompile_headers(${_asio_grpc_name} REUSE_FROM "asio-grpc-test-util-unifex${_asio_grpc_cxx_standard}")
    elseif(${_asio_grpc_type} STREQUAL "STDEXEC")
        target_link_libraries(${_asio_grpc_name} PRIVATE "asio-grpc-test-util-stdexec${_asio_grpc_cxx_standard}")

        target_precompile_headers(${_asio_grpc_name} REUSE_FROM "asio-grpc-test-util-stdexec${_asio_grpc_cxx_standard}")
    endif()

    if(ASIO_GRPC_DISCOVER_TESTS)
        doctest_discover_tests(${_asio_grpc_name})
    endif()
endfunction()

set(ASIO_GRPC_CPP17_TEST_SOURCE_FILES
    "test_alarm_17.cpp"
    "test_asio_grpc_17.cpp"
    "test_notify_when_done_17.cpp"
    "test_repeatedly_request_17.cpp"
    "test_bind_allocator_17.cpp"
    "test_grpc_context_17.cpp"
    "test_run_17.cpp"
    "test_generic_rpc_17.cpp"
    "test_grpc_stream_17.cpp"
    "test_test_17.cpp"
    "test_health_check_service_17.cpp"
    "test_client_rpc_17.cpp"
    "test_server_rpc_17.cpp"
    "test_waiter_17.cpp")
set(ASIO_GRPC_CPP20_TEST_SOURCE_FILES
    "test_asio_grpc_20.cpp" "test_repeatedly_request_20.cpp" "test_bind_allocator_20.cpp" "test_grpc_context_20.cpp"
    "test_grpc_stream_20.cpp" "test_server_rpc_20.cpp")
set_source_files_properties("test_test_17.cpp" PROPERTIES SKIP_UNITY_BUILD_INCLUSION on)

asio_grpc_add_test(asio-grpc-test-boost-cpp17 "BOOST_ASIO" "17" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES})
asio_grpc_add_test(asio-grpc-test-cpp17 "STANDALONE_ASIO" "17" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES})

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_test(asio-grpc-test-boost-cpp20 "BOOST_ASIO" "20" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES}
                       ${ASIO_GRPC_CPP20_TEST_SOURCE_FILES})
    asio_grpc_add_test(asio-grpc-test-cpp20 "STANDALONE_ASIO" "20" ${ASIO_GRPC_CPP17_TEST_SOURCE_FILES}
                       ${ASIO_GRPC_CPP20_TEST_SOURCE_FILES})
    asio_grpc_add_test(asio-grpc-test-unifex-cpp20 "UNIFEX" "20" "test_unifex_20.cpp")
endif()

if(ASIO_GRPC_ENABLE_STDEXEC_TESTS)
    asio_grpc_add_test(asio-grpc-test-stdexec "STDEXEC" "20" "test_stdexec.cpp")
    target_link_libraries(asio-grpc-test-stdexec PRIVATE asio-grpc-stdexec)
endif()

unset(ASIO_GRPC_CPP17_TEST_SOURCE_FILES)
unset(ASIO_GRPC_CPP20_TEST_SOURCE_FILES)
