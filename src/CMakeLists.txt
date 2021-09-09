# asio-grpc
add_library(asio-grpc INTERFACE)
add_library(asio-grpc::asio-grpc ALIAS asio-grpc)

unset(ASIO_GRPC_HEADER_FILES)
list(
    APPEND
    ASIO_GRPC_HEADER_FILES
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/asioGrpc.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/grpcContext.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/grpcContext.ipp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/grpcExecutor.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/initiate.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/rpcs.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/asioForward.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/attributes.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/functional.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcCompletionQueueEvent.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcContextImplementation.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/agrpc/detail/grpcContextImplementation.ipp"
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
unset(ASIO_GRPC_HEADER_FILES)

target_compile_features(asio-grpc INTERFACE cxx_std_17)

target_include_directories(asio-grpc INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
                                               $<INSTALL_INTERFACE:include>)

target_link_libraries(asio-grpc INTERFACE gRPC::grpc++ Boost::headers)

# asio-grpc objects
if(ASIO_GRPC_BUILD_TESTS)
    add_library(asio-grpc-objects OBJECT)

    target_sources(asio-grpc-objects PRIVATE asioGrpc.cpp)

    target_link_libraries(asio-grpc-objects PUBLIC asio-grpc-common-compile-options)

    if(ASIO_GRPC_ENABLE_DYNAMIC_ANALYSIS)
        target_compile_options(asio-grpc-objects PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)

        target_link_libraries(asio-grpc-objects PUBLIC asan ubsan)

        target_compile_definitions(asio-grpc-objects PUBLIC GRPC_ASAN_SUPPRESSED GRPC_TSAN_SUPPRESSED)
    endif()
endif()

# installation
if(ASIO_GRPC_INSTALL)
    include(GNUInstallDirs)

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake" ARCH_INDEPENDENT
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/asio-grpcConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/asio-grpc")

    install(
        TARGETS asio-grpc
        EXPORT asio-grpc_EXPORT_TARGETS
        PUBLIC_HEADER)

    install(
        EXPORT asio-grpc_EXPORT_TARGETS
        NAMESPACE asio-grpc::
        FILE asio-grpcConfig.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/asio-grpc")

    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/agrpc"
        TYPE INCLUDE
        FILES_MATCHING
        REGEX "[.hpp|.ipp]$")
endif()
