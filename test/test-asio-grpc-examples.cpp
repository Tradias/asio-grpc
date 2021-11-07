// Copyright 2021 Dennis Hezel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utils/freePort.hpp"

#include <boost/process/child.hpp>
#include <doctest/doctest.h>

#include <iostream>
#include <thread>

namespace test_asio_grpc_examples
{
TEST_SUITE_BEGIN("Examples" * doctest::timeout(180.0));

TEST_CASE("examples")
{
    const auto port = std::to_string(test::get_free_port());
    const char* server_program{};
    const char* client_program{};
    SUBCASE("Boost.Asio hello world")
    {
        client_program = ASIO_GRPC_EXAMPLE_HELLO_WORLD_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_HELLO_WORLD_SERVER;
    }
    SUBCASE("Boost.Asio streaming")
    {
        client_program = ASIO_GRPC_EXAMPLE_STREAMING_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_STREAMING_SERVER;
    }
    SUBCASE("unifex")
    {
        client_program = ASIO_GRPC_EXAMPLE_UNIFEX_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_UNIFEX_SERVER;
    }
    boost::process::child server(server_program, port);
    REQUIRE(server.valid());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    boost::process::child client(client_program, port);
    REQUIRE(client.valid());
    server.join();
    client.join();
    CHECK_EQ(0, server.exit_code());
    CHECK_EQ(0, client.exit_code());
}

TEST_SUITE_END();
}  // namespace test_asio_grpc_examples