set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

if(PORT MATCHES "(boost-cobalt)")
    set(VCPKG_C_FLAGS "")
    set(VCPKG_CXX_FLAGS "-std=c++20 -fcoroutines")
endif()
