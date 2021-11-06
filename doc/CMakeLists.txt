# Copyright 2021 Dennis Hezel
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
    add_executable(asio-grpc-documentation-${_asio_grpc_name})

    target_sources(asio-grpc-documentation-${_asio_grpc_name} PRIVATE ${_asio_grpc_name}.cpp
                                                                      ${ASIO_GRPC_GENERATED_EXAMPLE_SOURCES})

    target_link_libraries(asio-grpc-documentation-${_asio_grpc_name} PRIVATE asio-grpc-example-helper)
endfunction()

asio_grpc_add_documentation(server OBJECT)
target_link_libraries(asio-grpc-documentation-server PRIVATE Boost::coroutine Boost::thread asio-grpc-example-protos)

asio_grpc_add_documentation(client OBJECT)
target_link_libraries(asio-grpc-documentation-client PRIVATE Boost::coroutine Boost::thread asio-grpc-example-protos)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_documentation(blog OBJECT)
    target_link_libraries(asio-grpc-documentation-blog PRIVATE asio-grpc-example-protos-cpp20
                                                               asio-grpc-cpp20-compile-options)
endif()
