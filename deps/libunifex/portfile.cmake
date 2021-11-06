vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    facebookexperimental/libunifex
    REF
    e9e6dd5250477dd4c8502862cdb3238175302405
    SHA512
    5c3b0412e1cc83641d1594ab532d86a6599c3664f8c8b2a334ac3b22f2c7dd6c2f9d3281012805bfe1c0bff0f46e1e1f4a15ebf2dd973c5b7a14a255a528bf46
    HEAD_REF
    master
    PATCHES
    fix-install.patch
    allow-warnings.patch
    fix-execute-forward-declaration.patch)

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
