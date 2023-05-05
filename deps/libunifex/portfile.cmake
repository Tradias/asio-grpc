vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    facebookexperimental/libunifex
    REF
    v0.2.0
    SHA512
    94597f4267eaf04d9350e3d07916615bdc7a04029258c1c024099b7554503e6c245cd28e271dcd819700035a19e5e600aa54c9d418333ddb8ef8c10a6025c30e
    HEAD_REF
    main
    PATCHES
    fix-compile-error.patch
    do-not-link-std-coroutines.patch)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS
    FEATURE_OPTIONS
    FEATURES
    test
    BUILD_TESTING
    test
    UNIFEX_BUILD_EXAMPLES)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH} OPTIONS ${FEATURE_OPTIONS} -DCMAKE_CXX_STANDARD:STRING=20)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME unifex CONFIG_PATH lib/cmake/unifex)
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

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
