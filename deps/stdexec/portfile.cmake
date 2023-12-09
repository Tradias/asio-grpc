vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    NVIDIA/stdexec
    REF
    e05f158445ef957dc53731607d417ec6a7d834d7
    SHA512
    cd7aac8c5af8dcae99bf9d2e06f17f3546a776f479f8ccc6b0f834de1dc43a0d9f2cdfe3da5f115decf8bee7d8c01d2750be23e7ff21b581d5f28f272f21e3e7
    HEAD_REF
    main)

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH_RAPIDS
    REPO
    rapidsai/rapids-cmake
    REF
    c7a28304639a2ed460181b4753f3280c7833c718
    SHA512
    9a87fdef490199337778b8c9b4df31ca37d65df23803d058f13b406dcfda4d96d992b2780b0b878b61b027c0dc848351496a0f32e779f95298f259bab040b49b
    HEAD_REF
    main)

vcpkg_download_distfile(
    RAPIDS_cmake
    URLS
    "https://raw.githubusercontent.com/rapidsai/rapids-cmake/branch-23.02/RAPIDS.cmake"
    FILENAME
    "RAPIDS.cmake"
    SHA512
    e7830364222a9ea46fe7756859dc8d36e401c720f6a49880a2945a9ebc5bd9aa7e40a8bd382e1cae3af4235d5c9a7998f38331e23b676af7c5c72e7f00e61f0c
)
file(COPY "${RAPIDS_cmake}" DESTINATION "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/")

vcpkg_download_distfile(
    execution_bs
    URLS
    "https://raw.githubusercontent.com/brycelelbach/wg21_p2300_execution/f3f5531362cf6a9207e2e7be6c42e8bee9f41b07/execution.bs"
    FILENAME
    "execution.bs"
    SHA512
    f57fa1f964d18c39ecd4038a35fbeef25a6bba72854d37ed407fc598452c2c91a80e4448fe97566d64fe4853f370fe2215b519d787f85d2649ed3cd0bfe4016f
)
file(COPY "${execution_bs}" DESTINATION "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/")

set(VCPKG_BUILD_TYPE release)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS -DSTDEXEC_BUILD_TESTS=OFF -DSTDEXEC_BUILD_EXAMPLES=OFF
                      -DFETCHCONTENT_SOURCE_DIR_RAPIDS-CMAKE="${SOURCE_PATH_RAPIDS}")

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/stdexec)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
