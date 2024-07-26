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

# test main
function(asio_grpc_add_test_main _asio_grpc_name _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(
        ${_asio_grpc_name}
        PRIVATE # cmake-format: sort
                "utils/client_context.cpp"
                "utils/client_context.hpp"
                "utils/doctest.hpp"
                "utils/free_port.cpp"
                "utils/free_port.hpp"
                "utils/grpc_format.cpp"
                "utils/grpc_format.hpp"
                "utils/main.cpp"
                "utils/server_shutdown_initiator.cpp"
                "utils/server_shutdown_initiator.hpp"
                "utils/time.cpp"
                "utils/time.hpp")

    target_include_directories(${_asio_grpc_name} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(
        ${_asio_grpc_name}
        PUBLIC doctest::doctest GTest::gmock asio-grpc-compile-options${_asio_grpc_cxx_standard}
               asio-grpc-test-protos${_asio_grpc_cxx_standard} asio-grpc-coverage-options
        PRIVATE Boost::headers Boost::filesystem $<$<PLATFORM_ID:Linux>:rt> # for Boost.Interprocess
    )
endfunction()

set_source_files_properties("utils/main.cpp" PROPERTIES SKIP_UNITY_BUILD_INCLUSION on)

asio_grpc_add_test_main(asio-grpc-test-main "17")
asio_grpc_add_test_main(asio-grpc-test-main-cpp20 "20")

# test utils
function(asio_grpc_add_test_util _asio_grpc_name _asio_grpc_type _asio_grpc_cxx_standard)
    add_library(${_asio_grpc_name} OBJECT)

    target_sources(
        ${_asio_grpc_name}
        PRIVATE # cmake-format: sort
                "utils/asio_separate_compilation.cpp"
                "utils/asio_utils.cpp"
                "utils/asio_utils.hpp"
                "utils/grpc_client_server_test.cpp"
                "utils/grpc_client_server_test.hpp"
                "utils/grpc_client_server_test_base.cpp"
                "utils/grpc_client_server_test_base.hpp"
                "utils/grpc_context_test.cpp"
                "utils/grpc_context_test.hpp"
                "utils/grpc_generic_client_server_test.cpp"
                "utils/grpc_generic_client_server_test.hpp"
                "utils/test.cpp"
                "utils/test.hpp")

    target_include_directories(${_asio_grpc_name} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

    target_precompile_headers(${_asio_grpc_name} PRIVATE "utils/precompiled_header.hpp")

    if(${_asio_grpc_type} STREQUAL "BOOST_ASIO")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc Boost::coroutine Boost::thread)

        set(_test_suite_name "Boost.Asio")
        if(ARGN)
            target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-${ARGN})

            set(_test_suite_name "${_test_suite_name} ${ARGN}")
        endif()

        target_compile_definitions(${_asio_grpc_name} PUBLIC BOOST_ASIO_SEPARATE_COMPILATION BOOST_ASIO_NO_TS_EXECUTORS)

        target_sources(${_asio_grpc_name} PRIVATE "utils/io_context_test.cpp" "utils/io_context_test.hpp"
                                                  "utils/rpc.cpp" "utils/rpc.hpp")
    elseif(${_asio_grpc_type} STREQUAL "STANDALONE_ASIO")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-standalone-asio Boost::coroutine Boost::thread)

        set(_test_suite_name "Standalone Asio")
        if(ARGN)
            target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-${ARGN})

            set(_test_suite_name "${_test_suite_name} ${ARGN}")
        endif()

        target_compile_definitions(${_asio_grpc_name} PUBLIC ASIO_SEPARATE_COMPILATION ASIO_NO_TS_EXECUTORS)

        target_sources(${_asio_grpc_name} PRIVATE "utils/io_context_test.cpp" "utils/io_context_test.hpp"
                                                  "utils/rpc.cpp" "utils/rpc.hpp")
    elseif(${_asio_grpc_type} STREQUAL "UNIFEX")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-unifex)

        set(_test_suite_name "unifex")
    elseif(${_asio_grpc_type} STREQUAL "STDEXEC")
        target_link_libraries(${_asio_grpc_name} PUBLIC asio-grpc-stdexec)

        set(_test_suite_name "stdexec")
    endif()

    target_compile_definitions(
        ${_asio_grpc_name} PUBLIC "ASIO_GRPC_TEST_CPP_VERSION=\"${_test_suite_name} C++${_asio_grpc_cxx_standard}\""
                                  $<$<BOOL:${ASIO_GRPC_HAS_STD_PMR}>:AGRPC_TEST_HAS_STD_PMR>)

    convert_to_cpp_suffix(${_asio_grpc_cxx_standard})

    target_link_libraries(
        ${_asio_grpc_name} PUBLIC asio-grpc-test-main${_asio_grpc_cxx_standard}
                                  asio-grpc-test-protos${_asio_grpc_cxx_standard} asio-grpc-coverage-options)
endfunction()

asio_grpc_add_test_util(asio-grpc-test-util-boost-asio "BOOST_ASIO" "17")
asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio "STANDALONE_ASIO" "17")

if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_test_util(asio-grpc-test-util-boost-asio-cpp20 "BOOST_ASIO" "20")
    asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio-cpp20 "STANDALONE_ASIO" "20")
    asio_grpc_add_test_util(asio-grpc-test-util-unifex-cpp20 "UNIFEX" "20")
    asio_grpc_add_test_util(asio-grpc-test-util-boost-asio-unifex-cpp20 "BOOST_ASIO" "20" "unifex")
endif()

if(ASIO_GRPC_ENABLE_STDEXEC_TESTS)
    asio_grpc_add_test_util(asio-grpc-test-util-stdexec-cpp20 "STDEXEC" "20")

    if(ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
        asio_grpc_add_test_util(asio-grpc-test-util-standalone-asio-stdexec-cpp20 "STANDALONE_ASIO" "20" "stdexec")
    endif()
endif()
