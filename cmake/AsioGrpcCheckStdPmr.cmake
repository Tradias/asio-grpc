# Copyright 2025 Dennis Hezel
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(NOT DEFINED ASIO_GRPC_HAS_STD_PMR)
    try_compile(
        ASIO_GRPC_HAS_STD_PMR "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/check_std_pmr.cpp"
        CMAKE_FLAGS "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}" CXX_STANDARD 17 CXX_STANDARD_REQUIRED on)
endif()

message(STATUS "ASIO_GRPC_HAS_STD_PMR: ${ASIO_GRPC_HAS_STD_PMR}")
