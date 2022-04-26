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

# /* [asio_grpc_protobuf_generate] */
#[=======================================================================[.rst:
asio_grpc_protobuf_generate
------------

Add custom commands to process ``.proto`` files to C++::

asio_grpc_protobuf_generate(PROTOS <proto_file1> [<proto_file2>...]
                            [OUT_DIR <output_directory>]
                            [OUT_VAR <output_variable>]
                            [TARGET <target>]
                            [USAGE_REQUIREMENT PRIVATE|PUBLIC|INTERFACE]
                            [IMPORT_DIRS <directories>...]
                            [EXTRA_ARGS <arguments>...]
                            [GENERATE_GRPC]
                            [GENERATE_DESCRIPTORS]
                            [GENERATE_MOCK_CODE])

``PROTOS``
    ``.proto`` files
``OUT_DIR``
    Generated files output directory. Default: CMAKE_CURRENT_BINARY_DIR
``OUT_VAR``
    Variable to define with generated source files
``TARGET``
    Add generated source files to target.
``USAGE_REQUIREMENT``
    How to add sources to ``<target>``: ``PRIVATE``, ``PUBLIC``, ``INTERFACE``
    Default: ``PRIVATE``
``IMPORT_DIRS``
    Import directories to be added to the protoc command line. If unspecified
    then the directory of each .proto file will be used.
``EXTRA_ARGS``
    Additional protoc command line arguments
``GENERATE_GRPC``
    Generate grpc files
``GENERATE_DESCRIPTORS``
    Generate descriptor files named <proto_file_base_name>.desc
``GENERATE_MOCK_CODE``
    Generate gRPC client stub mock files named _mock.grpc.pb.h

#]=======================================================================]
# /* [asio_grpc_protobuf_generate] */
function(asio_grpc_protobuf_generate)
    include(CMakeParseArguments)

    set(_asio_grpc_options GENERATE_GRPC GENERATE_DESCRIPTORS GENERATE_MOCK_CODE)
    set(_asio_grpc_singleargs OUT_VAR OUT_DIR TARGET USAGE_REQUIREMENT)
    set(_asio_grpc_multiargs PROTOS IMPORT_DIRS EXTRA_ARGS)

    cmake_parse_arguments(asio_grpc_protobuf_generate "${_asio_grpc_options}" "${_asio_grpc_singleargs}"
                          "${_asio_grpc_multiargs}" "${ARGN}")

    if(asio_grpc_protobuf_generate_UNPARSED_ARGUMENTS)
        message(
            AUTHOR_WARNING
                "asio_grpc_protobuf_generate unknown argument: ${asio_grpc_protobuf_generate_UNPARSED_ARGUMENTS}")
    endif()

    if(asio_grpc_protobuf_generate_KEYWORDS_MISSING_VALUES)
        message(
            AUTHOR_WARNING
                "asio_grpc_protobuf_generate missing values for: ${asio_grpc_protobuf_generate_KEYWORDS_MISSING_VALUES}"
        )
    endif()

    if(NOT asio_grpc_protobuf_generate_PROTOS)
        message(SEND_ERROR "asio_grpc_protobuf_generate called without any proto files: PROTOS")
        return()
    endif()

    if(NOT asio_grpc_protobuf_generate_OUT_VAR AND NOT asio_grpc_protobuf_generate_TARGET)
        message(SEND_ERROR "asio_grpc_protobuf_generate called without a target or output variable: TARGET or OUT_VAR")
        return()
    endif()

    if(asio_grpc_protobuf_generate_TARGET AND NOT TARGET ${asio_grpc_protobuf_generate_TARGET})
        message(
            SEND_ERROR
                "asio_grpc_protobuf_generate argument passed to TARGET is not a target: ${asio_grpc_protobuf_generate_TARGET}"
        )
        return()
    endif()

    if(asio_grpc_protobuf_generate_GENERATE_MOCK_CODE AND NOT asio_grpc_protobuf_generate_GENERATE_GRPC)
        message(SEND_ERROR "asio_grpc_protobuf_generate argument GENERATE_MOCK_CODE requires GENERATE_GRPC")
        return()
    endif()

    if(NOT asio_grpc_protobuf_generate_OUT_DIR)
        set(asio_grpc_protobuf_generate_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    set(_asio_grpc_generated_extensions ".pb.cc" ".pb.h")
    if(asio_grpc_protobuf_generate_GENERATE_GRPC)
        list(APPEND _asio_grpc_generated_extensions ".grpc.pb.cc" ".grpc.pb.h")
        if(asio_grpc_protobuf_generate_GENERATE_MOCK_CODE)
            list(APPEND _asio_grpc_generated_extensions "_mock.grpc.pb.h")
        endif()
    endif()

    if(NOT asio_grpc_protobuf_generate_USAGE_REQUIREMENT)
        set(asio_grpc_protobuf_generate_USAGE_REQUIREMENT "PRIVATE")
    endif()

    if(NOT asio_grpc_protobuf_generate_IMPORT_DIRS)
        # Create an include path for each file specified
        foreach(_asio_grpc_file ${asio_grpc_protobuf_generate_PROTOS})
            get_filename_component(_asio_grpc_abs_file "${_asio_grpc_file}" ABSOLUTE)
            get_filename_component(_asio_grpc_abs_path "${_asio_grpc_abs_file}" PATH)
            list(APPEND asio_grpc_protobuf_generate_IMPORT_DIRS "${_asio_grpc_abs_path}")
        endforeach()
    endif()

    # Add all explicitly specified import directory to the include path
    foreach(_asio_grpc_dir ${asio_grpc_protobuf_generate_IMPORT_DIRS})
        get_filename_component(_asio_grpc_abs_dir "${_asio_grpc_dir}" ABSOLUTE)
        list(FIND _asio_grpc_protobuf_include_args "${_asio_grpc_abs_dir}" _asio_grpc_contains_already)
        if(${_asio_grpc_contains_already} EQUAL -1)
            list(APPEND _asio_grpc_protobuf_include_args -I "${_asio_grpc_abs_dir}")
        endif()
    endforeach()

    set(_asio_grpc_abs_input_files)
    set(_asio_grpc_generated_srcs)
    foreach(_asio_grpc_proto ${asio_grpc_protobuf_generate_PROTOS})
        get_filename_component(_asio_grpc_abs_file "${_asio_grpc_proto}" ABSOLUTE)
        get_filename_component(_asio_grpc_basename "${_asio_grpc_proto}" NAME_WE)

        list(APPEND _asio_grpc_abs_input_files "${_asio_grpc_abs_file}")

        # Compute generated file output directory
        set(_asio_grpc_proto_dir_lowest_path_count 999999999999)
        set(_asio_grpc_actual_our_dir)
        foreach(_asio_grpc_dir ${asio_grpc_protobuf_generate_IMPORT_DIRS})
            file(RELATIVE_PATH _asio_grpc_proto_file_relative_path "${_asio_grpc_dir}" "${_asio_grpc_abs_file}")
            file(TO_CMAKE_PATH "${_asio_grpc_proto_file_relative_path}" _asio_grpc_proto_file_relative_cmake_path)
            get_filename_component(_asio_grpc_proto_dir_relative_cmake_path
                                   "${_asio_grpc_proto_file_relative_cmake_path}" PATH)
            string(REGEX MATCHALL "/" _asio_grpc_proto_dir_path_list "${_asio_grpc_proto_dir_relative_cmake_path}")
            list(LENGTH _asio_grpc_proto_dir_path_list _asio_grpc_proto_dir_path_count)
            if(${_asio_grpc_proto_dir_path_count} LESS ${_asio_grpc_proto_dir_lowest_path_count})
                set(_asio_grpc_proto_dir_lowest_path_count ${_asio_grpc_proto_dir_path_count})
                set(_asio_grpc_actual_our_dir
                    "${asio_grpc_protobuf_generate_OUT_DIR}/${_asio_grpc_proto_dir_relative_cmake_path}")
            endif()
        endforeach()

        # Collect generated files
        foreach(_asio_grpc_ext ${_asio_grpc_generated_extensions})
            list(APPEND _asio_grpc_generated_srcs
                 "${_asio_grpc_actual_our_dir}/${_asio_grpc_basename}${_asio_grpc_ext}")
        endforeach()

        if(asio_grpc_protobuf_generate_GENERATE_DESCRIPTORS)
            set(_asio_grpc_descriptor_file "${_asio_grpc_actual_our_dir}/${_asio_grpc_basename}.desc")
            set(_asio_grpc_descriptor_command "--descriptor_set_out=${_asio_grpc_descriptor_file}")
            list(APPEND _asio_grpc_generated_srcs "${_asio_grpc_descriptor_file}")
        endif()
    endforeach()

    # Run protoc
    set(_asio_grpc_command_arguments --cpp_out "${asio_grpc_protobuf_generate_OUT_DIR}"
                                     "${_asio_grpc_descriptor_command}" "${_asio_grpc_protobuf_include_args}")
    if(asio_grpc_protobuf_generate_GENERATE_GRPC)
        if(asio_grpc_protobuf_generate_GENERATE_MOCK_CODE)
            set(_asio_grpc_generate_mock_code "generate_mock_code=true:")
        endif()
        list(APPEND _asio_grpc_command_arguments --grpc_out
             "${_asio_grpc_generate_mock_code}${asio_grpc_protobuf_generate_OUT_DIR}"
             "--plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
    endif()
    list(APPEND _asio_grpc_command_arguments ${asio_grpc_protobuf_generate_EXTRA_ARGS} ${_asio_grpc_abs_input_files})
    string(REPLACE ";" " " _asio_grpc_pretty_command_arguments "${_asio_grpc_command_arguments}")
    add_custom_command(
        OUTPUT ${_asio_grpc_generated_srcs}
        COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${asio_grpc_protobuf_generate_OUT_DIR}"
        COMMAND protobuf::protoc ${_asio_grpc_command_arguments}
        DEPENDS protobuf::protoc ${_asio_grpc_abs_input_files}
        COMMENT "protoc ${_asio_grpc_pretty_command_arguments}"
        VERBATIM)

    set_source_files_properties(${_asio_grpc_generated_srcs} PROPERTIES SKIP_UNITY_BUILD_INCLUSION on)

    if(asio_grpc_protobuf_generate_TARGET)
        if("${asio_grpc_protobuf_generate_USAGE_REQUIREMENT}" STREQUAL "INTERFACE")
            target_sources(${asio_grpc_protobuf_generate_TARGET} INTERFACE ${_asio_grpc_generated_srcs})
        else()
            target_sources(${asio_grpc_protobuf_generate_TARGET} PRIVATE ${_asio_grpc_generated_srcs})
        endif()

        if("${asio_grpc_protobuf_generate_USAGE_REQUIREMENT}" STREQUAL "PUBLIC")
            target_include_directories(${asio_grpc_protobuf_generate_TARGET}
                                       PUBLIC "$<BUILD_INTERFACE:${asio_grpc_protobuf_generate_OUT_DIR}>")
        else()
            target_include_directories(
                ${asio_grpc_protobuf_generate_TARGET} "${asio_grpc_protobuf_generate_USAGE_REQUIREMENT}"
                "${asio_grpc_protobuf_generate_OUT_DIR}")
        endif()
    endif()

    if(asio_grpc_protobuf_generate_OUT_VAR)
        set(${asio_grpc_protobuf_generate_OUT_VAR}
            ${_asio_grpc_generated_srcs}
            PARENT_SCOPE)
    endif()
endfunction()
