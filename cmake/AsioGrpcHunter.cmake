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

set(HUNTER_PACKAGES gRPC)

if(ASIO_GRPC_HUNTER_BACKEND_BOOST_ASIO OR ASIO_GRPC_BUILD_TESTS)
    list(APPEND HUNTER_PACKAGES Boost)
endif()

if(ASIO_GRPC_HUNTER_BACKEND_STANDALONE_ASIO OR ASIO_GRPC_BUILD_TESTS)
    list(APPEND HUNTER_PACKAGES asio)
endif()

if(ASIO_GRPC_USE_BOOST_CONTAINER)
    set(HUNTER_Boost_COMPONENTS container)
endif()

if(ASIO_GRPC_BUILD_TESTS)
    list(APPEND HUNTER_PACKAGES doctest)
    list(APPEND HUNTER_Boost_COMPONENTS coroutine thread filesystem)
endif()

include(FetchContent)
fetchcontent_declare(SetupHunter GIT_REPOSITORY https://github.com/cpp-pm/gate)
fetchcontent_makeavailable(SetupHunter)
