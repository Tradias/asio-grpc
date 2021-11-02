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

asio_grpc_add_example(server-documentation)
target_link_libraries(asio-grpc-server-documentation PRIVATE Boost::coroutine Boost::thread asio-grpc-example-protos)

asio_grpc_add_example(client-documentation)
target_link_libraries(asio-grpc-client-documentation PRIVATE Boost::coroutine Boost::thread asio-grpc-example-protos)

if(ASIO_GRPC_BOOST_ASIO_HAS_CO_AWAIT AND ASIO_GRPC_ENABLE_CPP20_TESTS_AND_EXAMPLES)
    asio_grpc_add_example(blog)
    target_link_libraries(asio-grpc-blog PRIVATE asio-grpc-example-protos-cpp20 asio-grpc-cpp20-compile-options)
endif()
