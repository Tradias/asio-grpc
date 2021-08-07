# Adapted from the original protobuf_generate provided by protobuf-config.cmake
function(asio_grpc_protobuf_generate)
    include(CMakeParseArguments)

    set(_options GENERATE_GRPC)
    set(_singleargs OUT_VAR PROTOC_OUT_DIR)
    set(_multiargs PROTOS IMPORT_DIRS)

    cmake_parse_arguments(asio_grpc_protobuf_generate "${_options}" "${_singleargs}" "${_multiargs}" "${ARGN}")

    if(NOT asio_grpc_protobuf_generate_PROTOS)
        message(SEND_ERROR "Error: asio_grpc_protobuf_generate called without any targets or source files")
        return()
    endif()

    if(NOT asio_grpc_protobuf_generate_OUT_VAR)
        message(SEND_ERROR "Error: asio_grpc_protobuf_generate called without a target or output variable")
        return()
    endif()

    if(NOT asio_grpc_protobuf_generate_PROTOC_OUT_DIR)
        set(asio_grpc_protobuf_generate_PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    set(GENERATED_EXTENSIONS .pb.cc .pb.h)
    if(asio_grpc_protobuf_generate_GENERATE_GRPC)
        list(APPEND GENERATED_EXTENSIONS .grpc.pb.cc .grpc.pb.h)
    endif()

    # Create an include path for each file specified
    foreach(_file ${asio_grpc_protobuf_generate_PROTOS})
        get_filename_component(_abs_file ${_file} ABSOLUTE)
        get_filename_component(_abs_path ${_abs_file} PATH)
        list(FIND _protobuf_include_path ${_abs_path} _contains_already)
        if(${_contains_already} EQUAL -1)
            list(APPEND _protobuf_include_path -I ${_abs_path})
        endif()
    endforeach()

    # Add all explicitly specified import directory to the include path
    foreach(DIR ${asio_grpc_protobuf_generate_IMPORT_DIRS})
        get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
        list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
        if(${_contains_already} EQUAL -1)
            list(APPEND _protobuf_include_path -I ${ABS_PATH})
        endif()
    endforeach()

    set(_generated_srcs_all)
    foreach(_proto ${asio_grpc_protobuf_generate_PROTOS})
        get_filename_component(_abs_file ${_proto} ABSOLUTE)
        get_filename_component(_basename ${_proto} NAME_WE)

        get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
        get_filename_component(_proto_dir ${_abs_dir} NAME)
        set(_out_dir "${asio_grpc_protobuf_generate_PROTOC_OUT_DIR}/${_proto_dir}")

        set(_generated_srcs)
        foreach(_ext ${GENERATED_EXTENSIONS})
            list(APPEND _generated_srcs "${_out_dir}/${_basename}${_ext}")
        endforeach()
        list(APPEND _generated_srcs_all ${_generated_srcs})

        # Run protoc
        set(_command_arguments --cpp_out "${_out_dir}" ${_protobuf_include_path})
        if(asio_grpc_protobuf_generate_GENERATE_GRPC)
            list(APPEND _command_arguments --grpc_out "${_out_dir}"
                 "--plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
        endif()
        list(APPEND _command_arguments "${_abs_file}")
        string(REPLACE ";" " " _pretty_command_arguments "${_command_arguments}")
        add_custom_command(
            OUTPUT ${_generated_srcs}
            COMMAND ${CMAKE_COMMAND} "-E" "make_directory" "${_out_dir}"
            COMMAND protobuf::protoc ${_command_arguments}
            MAIN_DEPENDENCY "${_abs_file}"
            DEPENDS protobuf::protoc
            COMMENT "protoc ${_pretty_command_arguments}"
            VERBATIM)
    endforeach()

    set_source_files_properties(${_generated_srcs_all} PROPERTIES GENERATED on COMPILE_OPTIONS
                                                                               $<$<CXX_COMPILER_ID:MSVC>:/W1>)
    set(${asio_grpc_protobuf_generate_OUT_VAR}
        ${_generated_srcs_all}
        PARENT_SCOPE)
endfunction()
