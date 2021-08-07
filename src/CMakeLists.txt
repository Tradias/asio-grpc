# asio-grpc
add_library(asio-grpc INTERFACE)
add_library(asio-grpc::asio-grpc ALIAS asio-grpc)

unset(ASIO_GRPC_HEADER_FILES)
list(
    APPEND
    ASIO_GRPC_HEADER_FILES
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/asioGrpc.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/asioGrpcSrc.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/grpcContext.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/grpcExecutor.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/initiate.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/rpcs.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/asioForward.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/attributes.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/functional.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcCompletionQueueEvent.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcContextImplementation.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcContextInteraction.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcContextOperation.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcExecutorBase.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcExecutorOperation.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcExecutorOptions.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/initiate.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/memory.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/rpcs.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/utility.hpp")
set_target_properties(asio-grpc PROPERTIES ASIO_GRPC_HEADER_FILES "${ASIO_GRPC_HEADER_FILES}")

target_compile_features(asio-grpc INTERFACE cxx_std_17)

target_include_directories(asio-grpc INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(asio-grpc INTERFACE gRPC::grpc++ Boost::headers)

# asio-grpc objects
if(ASIO_GRPC_BUILD_TESTS)
    add_library(asio-grpc-objects OBJECT)

    target_sources(asio-grpc-objects PRIVATE asioGrpc.cpp)

    target_link_libraries(asio-grpc-objects PUBLIC asio-grpc)

    target_compile_options(asio-grpc-objects PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/W4>
                                                    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic-errors>)

    target_compile_definitions(
        asio-grpc-objects
        PUBLIC $<$<CXX_COMPILER_ID:MSVC>:BOOST_ASIO_HAS_WORKING_EXPRESSION_SFINAE _WIN32_WINNT=0x0A00 # Windows 10
               WINVER=0x0A00> BOOST_ASIO_NO_DEPRECATED GRPC_TSAN_SUPPRESSED GRPC_TSAN_SUPPRESSED)

    if(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS)
        target_compile_options(asio-grpc-objects PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)

        target_link_libraries(asio-grpc-objects PUBLIC asan ubsan)
    endif()
endif()

unset(ASIO_GRPC_HEADER_FILES)

# installation
if(ASIO_GRPC_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake" ARCH_INDEPENDENT
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake" DESTINATION share/asio-grpc)

    include(GNUInstallDirs)
    install(
        TARGETS asio-grpc
        EXPORT asio-grpc_EXPORT_TARGETS
        PUBLIC_HEADER)

    install(
        EXPORT asio-grpc_EXPORT_TARGETS
        NAMESPACE asio-grpc::
        FILE asio-grpcConfig.cmake
        DESTINATION share/asio-grpc)

    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/agrpc"
        TYPE INCLUDE
        FILES_MATCHING
        REGEX "[.hpp|.cpp]$")
endif()
