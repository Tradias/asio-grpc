find_package(Git)

function(asio_grpc_create_init_git_hooks_target)
    if(TARGET asio-grpc-init-git-hooks)
        return()
    endif()

    set(ASIO_GRPC_GIT_HOOKS_TARGET_DIR "${CMAKE_SOURCE_DIR}/.git/hooks")
    set(ASIO_GRPC_GIT_HOOKS_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/git-hooks/")

    if(NOT EXISTS "${ASIO_GRPC_GIT_HOOKS_TARGET_DIR}/pre-commit"
       OR NOT EXISTS "${ASIO_GRPC_GIT_HOOKS_TARGET_DIR}/AsioGrpcPreCommit.cmake")
        message(
            STATUS
                "Initialize clang-format and cmake-format pre-commit hooks by building CMake target asio-grpc-init-git-hooks."
        )
    endif()

    find_program(ASIO_GRPC_CMAKE_FORMAT_PROGRAM cmake-format)
    find_program(ASIO_GRPC_CLANG_FORMAT_PROGRAM clang-format)

    set(ASIO_GRPC_INIT_GIT_HOOKS_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/pre-commit.in"
                                         "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/AsioGrpcPreCommit.cmake.in")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/pre-commit.in"
                   "${ASIO_GRPC_GIT_HOOKS_SOURCE_DIR}/pre-commit" @ONLY)
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/AsioGrpcPreCommit.cmake.in"
                   "${ASIO_GRPC_GIT_HOOKS_SOURCE_DIR}/AsioGrpcPreCommit.cmake" @ONLY)

    set(_asio_grpc_command_arguments
        "-DGIT_HOOKS_TARGET_DIR=${ASIO_GRPC_GIT_HOOKS_TARGET_DIR}"
        "-DGIT_HOOKS_SOURCE_DIR=${ASIO_GRPC_GIT_HOOKS_SOURCE_DIR}" -P
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AsioGrpcGitHooksInstaller.cmake")
    string(REPLACE ";" " " _asio_grpc_pretty_command_arguments "${_asio_grpc_command_arguments}")
    add_custom_target(
        asio-grpc-init-git-hooks
        DEPENDS ${ASIO_GRPC_INIT_GIT_HOOKS_SOURCES}
        SOURCES ${ASIO_GRPC_INIT_GIT_HOOKS_SOURCES}
        COMMAND ${CMAKE_COMMAND} ${_asio_grpc_command_arguments}
        COMMENT "cmake ${_asio_grpc_pretty_command_arguments}"
        VERBATIM)
endfunction()

if(GIT_FOUND)
    asio_grpc_create_init_git_hooks_target()
endif()
