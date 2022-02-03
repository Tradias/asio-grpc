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
            AUTHOR_WARNING
                "Initialize clang-format and cmake-format pre-commit hooks by building the CMake target asio-grpc-init-git-hooks."
        )
    endif()

    find_program(ASIO_GRPC_CMAKE_FORMAT_PROGRAM cmake-format)
    find_program(ASIO_GRPC_CLANG_FORMAT_PROGRAM clang-format)

    if(NOT ASIO_GRPC_CMAKE_FORMAT_PROGRAM OR NOT ASIO_GRPC_CLANG_FORMAT_PROGRAM)
        message(
            AUTHOR_WARNING
                "Cannot create init-git-hooks target with\ncmake-format: ${ASIO_GRPC_CMAKE_FORMAT_PROGRAM}\nclang-format: ${ASIO_GRPC_CLANG_FORMAT_PROGRAM}"
        )
        return()
    endif()

    set(ASIO_GRPC_INIT_GIT_HOOKS_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/pre-commit.in"
                                         "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/AsioGrpcPreCommit.cmake.in")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hooks/pre-commit.in"
                   "${ASIO_GRPC_GIT_HOOKS_SOURCE_DIR}/pre-commit" @ONLY NEWLINE_STYLE UNIX)
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
