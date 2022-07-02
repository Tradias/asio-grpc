vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    axboe/liburing
    REF
    liburing-2.2
    SHA512
    243e42b221115be7bc51d91d807aaaa545bb14709ceceee502a1b694a41bcef22ec660c11dc81eaddcc23a07748d6d6b5e8a13a670aed63b3aa660dfb849ac1e
    HEAD_REF
    master
    PATCHES
    fix-configure.patch # ignore unsupported options, handle ENABLE_SHARED
    disable-tests-and-examples.patch)

# note: check ${SOURCE_PATH}/liburing.spec before updating configure options
vcpkg_configure_make(SOURCE_PATH "${SOURCE_PATH}" COPY_SOURCE)
vcpkg_install_make()
vcpkg_fixup_pkgconfig()

file(
    INSTALL "${SOURCE_PATH}/LICENSE"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    RENAME copyright)
file(INSTALL "${CURRENT_PORT_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

# note: {SOURCE_PATH}/src/Makefile makes liburing.so from liburing.a. For dynamic, remove intermediate file liburing.a
# when install is finished.
if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    file(REMOVE "${CURRENT_PACKAGES_DIR}/debug/lib/liburing.a" "${CURRENT_PACKAGES_DIR}/lib/liburing.a")
endif()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/man")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/${PORT}/man2")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/${PORT}/man3")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/${PORT}/man7")
