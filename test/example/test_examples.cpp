// Copyright 2026 Dennis Hezel
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

#include "utils/free_port.hpp"

#include <boost/process/v2/process.hpp>
#include <doctest/doctest.h>

#include <thread>

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
    SUBCASE("Boost.Asio hello world arena")
    {
        client_program = ASIO_GRPC_EXAMPLE_HELLO_WORLD_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_HELLO_WORLD_SERVER_ARENA;
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
    SUBCASE("Boost.Asio main io_context")
    {
        client_program = ASIO_GRPC_EXAMPLE_SHARE_IO_CONTEXT_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_MAIN_IO_CONTEXT_SERVER;
        args.emplace_back(std::to_string(test::get_free_port()));
    }
#ifdef ASIO_GRPC_EXAMPLE_UNIFEX_CLIENT
    SUBCASE("unifex")
    {
        client_program = ASIO_GRPC_EXAMPLE_UNIFEX_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_UNIFEX_SERVER;
    }
#endif
    SUBCASE("generic")
    {
        client_program = ASIO_GRPC_EXAMPLE_GENERIC_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_GENERIC_SERVER;
    }
    SUBCASE("multi-threaded")
    {
        client_program = ASIO_GRPC_EXAMPLE_MULTI_THREADED_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_MULTI_THREADED_SERVER;
    }
    SUBCASE("multi-threaded-alternative")
    {
        client_program = ASIO_GRPC_EXAMPLE_MULTI_THREADED_ALTERNATIVE_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_MULTI_THREADED_ALTERNATIVE_SERVER;
    }
    SUBCASE("async-generator")
    {
        client_program = ASIO_GRPC_EXAMPLE_ASYNC_GENERATOR_CLIENT;
        server_program = ASIO_GRPC_EXAMPLE_ASYNC_GENERATOR_SERVER;
    }
    boost::asio::io_context io_context{1};
    boost::process::process server{io_context, server_program, args};
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    boost::process::process client(io_context, client_program, std::move(args));
    io_context.run();
    server.wait();
    client.wait();
    CHECK_EQ(0, server.exit_code());
    CHECK_EQ(0, client.exit_code());
}