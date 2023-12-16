# Copyright 2023 Dennis Hezel
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

if(ASIO_GRPC_ENABLE_CHECK_HEADER_SYNTAX_TARGET)
    add_subdirectory(syntax)
endif()

add_subdirectory(proto)

add_subdirectory(utils)

# add_subdirectory(src)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    add_subdirectory(example)
endif()

if(ASIO_GRPC_ENABLE_CMAKE_INSTALL_TEST)
    add_subdirectory(cmake)
endif()
