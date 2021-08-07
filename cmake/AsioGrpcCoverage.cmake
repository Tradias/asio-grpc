function(asio_grpc_add_coverage_flags _asio_grpc_target)
    target_compile_options(${_asio_grpc_target} PRIVATE --coverage)
    target_link_libraries(${_asio_grpc_target} PRIVATE gcov)
endfunction()

function(asio_grpc_coverage_report_for_target _asio_grpc_target _asio_grpc_source)
    get_filename_component(_asio_grpc_source_name "${_asio_grpc_source}" NAME)
    find_program(ASIO_GRPC_GCOV_PROGRAM gcov)
    add_custom_target(
        ${_asio_grpc_target}-coverage
        DEPENDS ${_asio_grpc_target}
        COMMAND
            "${ASIO_GRPC_GCOV_PROGRAM}" --relative-only --demangled-names --preserve-paths -o
            "$<TARGET_FILE_DIR:${_asio_grpc_target}>/CMakeFiles/${_asio_grpc_target}.dir/${_asio_grpc_source_name}.gcda"
            "${_asio_grpc_source}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()
