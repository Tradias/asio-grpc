# PR #137 — C++20 Module Support Todo

> Tracking work items derived from the PR comment by Tradias (repo owner) and the
> follow-up cleanup commit he pushed directly to this branch.
>
> PR: https://github.com/Tradias/asio-grpc/pull/137
> Tradias comment: "There are still a lot of things not working and the CMake story
> of installing modules is expectedly poorly developed. I have reduced things down
> to just the Boost.Asio based module, no health check, which seems to work with
> MSVC and Ninja (not with Visual Studio generator)."

---

## Group 1 — CMake / Build System

- [x] Add `ASIO_GRPC_BUILD_MODULE` option to `CMakeLists.txt` (replaces CMake version check)
- [x] Bump CMake max policy version from `3.27` → `4.3` in `CMakeLists.txt`
- [x] Add `CMAKE_CXX_SCAN_FOR_MODULES: FALSE` to `CMakePresets.json`
- [x] Rework `cmake/AsioGrpcInstallation.cmake` — gate on `ASIO_GRPC_BUILD_MODULE`, single target only, fix install `DESTINATION` to `${ASIO_GRPC_CMAKE_CONFIG_INSTALL_DIR}/src`
- [x] Add module target property block to `cmake/asio-grpcConfig.cmake.in`
- [x] Replace 8 backend-specific module targets with single `asio-grpc-module` OBJECT library in `src/CMakeLists.txt`
- [x] Add `-DASIO_GRPC_BUILD_MODULE=on` to MSVC/Ninja CI configure args in `.github/workflows/build.yml`

---

## Group 2 — Module Source File

- [x] Create `src/asio_grpc.cppm` with global-module-fragment pattern
- [x] Rewrite exports to use `export namespace agrpc { using ...; }` pattern
- [x] Add reactor/callback types to export list (`ReactorPtr`, `allocate_reactor`, `make_reactor`, etc.)
- [x] Add `#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)` guards for Asio-only exports
- [x] Add `#ifdef AGRPC_ASIO_HAS_CO_AWAIT` guard for coroutine exports
- [x] Remove `src/asio_grpc_health_check.cppm` — health check module deferred until core is stable

---

## Group 3 — Header Bug Fixes

- [x] Add 4 missing includes to `src/agrpc/asio_grpc.hpp` (`client_callback.hpp`, `reactor_ptr.hpp`, `register_yield_rpc_handler.hpp`, `server_callback.hpp`)
- [x] Fix `unstoppable_token` variable template in `src/agrpc/detail/execution_asio.hpp` (wrong default type `std::false_type` → `void`)
- [x] Add `UnstoppableToken` forward declaration + `IS_STOP_EVER_POSSIBLE_V` specialization in `src/agrpc/detail/association_asio.hpp`
- [x] Wrap `src/agrpc/register_yield_rpc_handler.hpp` with `#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)` guard

---

## Group 4 — Tests

- [x] Add `test/src/test_module.cpp` — doctest exercising `GrpcContext` + `Alarm` via `import asio_grpc`
- [x] Add `asio-grpc-test-cpp20-module` executable to `test/src/CMakeLists.txt` gated on `ASIO_GRPC_BUILD_MODULE`
- [x] Add `test/cmake/superbuild/src/cpp20_module.cpp` — cmake install test stub using `import asio_grpc`
- [x] Add `test/cmake/superbuild/src/cpp20_module_empty.cpp` — empty stub for non-module builds
- [x] Wire `cpp20_module()` call into `test/cmake/superbuild/src/main.cpp`
- [x] Add `cpp20-module` object library and link to `main` in `test/cmake/superbuild/src/CMakeLists.txt`
- [x] Propagate `ASIO_GRPC_BUILD_MODULE` into superbuild in `test/cmake/superbuild/CMakeLists.txt.in`
- [x] Remove hardcoded `Release` build config in `test/cmake/subdirectory/CMakeLists.txt.in`
- [x] Switch cmake install test generator to `Ninja` and add `--build-config` flag in `test/cmake/CMakeLists.txt`

---

## Open / Still To Do

- [x] **Visual Studio generator support** — guarded `HEADER_FILE_ONLY` workaround with
  `NOT CMAKE_GENERATOR MATCHES "Visual Studio"` in `src/CMakeLists.txt`. VS builds now
  compile the module normally (BMI is built but not installed, which is acceptable); non-VS
  generators continue to skip compilation via `HEADER_FILE_ONLY`.
- [ ] **Health check module** (`asio_grpc.health_check`) — removed from this PR to keep scope
  manageable. Should be re-added in a follow-up once the core Boost.Asio module is stable
  and merged.
- [ ] **Multi-backend module support** — currently only `asio-grpc` (Boost.Asio backend) is
  exposed via `asio-grpc-module`. The standalone-asio, unifex, and stdexec backends have no
  module targets yet. Follow-up work after core is stable.
