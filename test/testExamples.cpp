// Copyright 2022 Dennis Hezel
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

#include "utils/doctest.hpp"
#include "utils/freePort.hpp"

#include <boost/process/child.hpp>

#include <thread>

DOCTEST_TEST_SUITE("Examples")
{
TEST_CASE("examples")
{
    std::vector<std::string> args{std::to_string(test::get_free_port())};
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
    SUBCASE("Boost.Asio share io_context")
    {
        client_program = ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_SERVER;
        args.emplace_back(std::to_string(test::get_free_port()));
    }
#ifdef ASIO_GRPC_EXAMPLE_FILE_TRANSFER_CLIENT
    SUBCASE("Boost.Asio file transfer")
    {
        client_program = ASIO_GRPC_EXAMPLE_FILE_TRANSFER_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_FILE_TRANSFER_SERVER;
        args.emplace_back(ASIO_GRPC_EXAMPLE_TEMP_DIR);
    }
#endif
    SUBCASE("unifex")
    {
        client_program = ASIO_GRPC_EXAMPLE_UNIFEX_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_UNIFEX_SERVER;
    }
    SUBCASE("generic")
    {
        client_program = ASIO_GRPC_EXAMPLE_GENERIC_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_GENERIC_SERVER;
    }
    boost::process::child server(server_program, args);
    REQUIRE(server.valid());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    boost::process::child client(client_program, std::move(args));
    REQUIRE(client.valid());
    server.join();
    client.join();
    CHECK_EQ(0, server.exit_code());
    CHECK_EQ(0, client.exit_code());
}
}