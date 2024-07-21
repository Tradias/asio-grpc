#header-only library

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    chriskohlhoff/asio
    REF
    asio-1-17-0
    SHA512
    d99bbf2c4628e0ce43b21ccf5d4f75180c05e5fd84e394dd800f340b1c607f663b43f442253f5cac53394aee3b6bd17aa2082cd10637ac952a83f154ffab332e
    HEAD_REF
    master)

# Always use "ASIO_STANDALONE" to avoid boost dependency
vcpkg_replace_string("${SOURCE_PATH}/asio/include/asio/detail/config.hpp" "defined(ASIO_STANDALONE)"
                     "!defined(VCPKG_DISABLE_ASIO_STANDALONE)")

# CMake install
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

vcpkg_cmake_config_fixup()
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/asio-config.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/asio/LICENSE_1_0.txt")
