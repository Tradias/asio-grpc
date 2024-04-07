# Copyright 2024 Dennis Hezel
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

get_target_property(_asio_grpc_headers asio-grpc-sources INTERFACE_SOURCES)

set_source_files_properties(${_asio_grpc_headers} PROPERTIES LANGUAGE CXX)

add_library(asio-grpc-check-header-syntax OBJECT)

target_sources(asio-grpc-check-header-syntax PRIVATE ${_asio_grpc_headers})

target_compile_options(asio-grpc-check-header-syntax
                       PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/Zs;/WX,-Werror;-fsyntax-only;-Wno-unused-const-variable>)

target_link_libraries(asio-grpc-check-header-syntax PRIVATE asio-grpc-sources asio-grpc-compile-options asio-grpc)

set_target_properties(asio-grpc-check-header-syntax PROPERTIES EXCLUDE_FROM_ALL on)
