# Copyright 2022 Dennis Hezel
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

function(asio_grpc_add_documentation _asio_grpc_name)
    add_library(asio-grpc-documentation-${_asio_grpc_name} OBJECT)

    target_sources(asio-grpc-documentation-${_asio_grpc_name} PRIVATE ${ARGN})

    target_link_libraries(
        asio-grpc-documentation-${_asio_grpc_name} PRIVATE asio-grpc-example-helper asio-grpc-example-protos-cpp20
                                                           asio-grpc-cpp20-compile-options)
endfunction()

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_documentation(blog blog.cpp)

    asio_grpc_add_documentation(boost-asio client.cpp server.cpp)
    target_link_libraries(asio-grpc-documentation-boost-asio PRIVATE asio-grpc::asio-grpc)

    asio_grpc_add_documentation(unifex unifex-client.cpp unifex-server.cpp)
    target_link_libraries(asio-grpc-documentation-unifex PRIVATE asio-grpc::asio-grpc-unifex)
endif()
