vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    facebookexperimental/libunifex
    REF
    968b67c799ad3f4618f74b184323bb7d3c0fab48
    SHA512
    ddc04b69ae35f6c823bf15f5e2966dd7f1a4ca64dadad0713ddfcf316f6d8765d634dfc5a50c5d47e24586a8ba446914c2de6a9983e5da5be66b6297460b7106
    HEAD_REF
    master
    PATCHES
    fix-install.patch
    allow-warnings.patch
    fix-await_transform-forward-declaration.patch)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS
    FEATURE_OPTIONS
    FEATURES
    test
    BUILD_TESTING
    test
    UNIFEX_BUILD_EXAMPLES)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH} OPTIONS ${FEATURE_OPTIONS})
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME unifex CONFIG_PATH lib/cmake/unifex)
vcpkg_copy_pdbs()

file(
    INSTALL "${SOURCE_PATH}/LICENSE.txt"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    RENAME copyright)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share"
     "${CURRENT_PACKAGES_DIR}/include/unifex/config.hpp.in")
if(VCPKG_TARGET_IS_WINDOWS)
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/unifex/linux")
elseif(VCPKG_TARGET_IS_LINUX)
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/unifex/win32")
endif()
