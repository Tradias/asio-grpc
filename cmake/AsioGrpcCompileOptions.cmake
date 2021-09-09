# common compile options
add_library(asio-grpc-common-compile-options INTERFACE)

target_compile_options(
    asio-grpc-common-compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:
              /external:I
              $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
              /external:W1
              /external:templates-
              /W4>
              $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall
              -Wextra
              -pedantic-errors>)

target_compile_definitions(
    asio-grpc-common-compile-options
    INTERFACE $<$<CXX_COMPILER_ID:MSVC>:
              BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT
              BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT
              BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT
              _WIN32_WINNT=0x0A00 # Windows 10
              WINVER=0x0A00>
              BOOST_ASIO_NO_DEPRECATED)

target_link_libraries(asio-grpc-common-compile-options INTERFACE asio-grpc Boost::disable_autolinking)

# C++20 compile options
add_library(asio-grpc-cpp20-compile-options INTERFACE)

target_compile_features(asio-grpc-cpp20-compile-options INTERFACE cxx_std_20)

target_compile_options(asio-grpc-cpp20-compile-options INTERFACE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fcoroutines>)
