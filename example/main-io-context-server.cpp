// Copyright 2024 Dennis Hezel
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

#include "awaitable_server_rpc.hpp"
#include "example/v1/example.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <string_view>
#include <thread>

namespace asio = boost::asio;

// begin-snippet: server-side-main-io-context

// Example showing how to use an io_context as the main context and a GrpcContext on a separate thread for gRPC servers.

// end-snippet

//  A simple tcp request that will be handled by the io_context.
asio::awaitable<void> handle_tcp_request(asio::ip::port_type port)
{
    const auto& executor = co_await asio::this_coro::executor;
    asio::ip::tcp::acceptor acceptor(executor, {asio::ip::make_address_v4("127.0.0.1"), port});
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
    char data[128];
    const auto bytes_read = co_await socket.async_read_some(asio::buffer(data), asio::use_awaitable);

    abort_if_not("example" == std::string_view(data, bytes_read - 1));
}

// A unary RPC request that will be handled by the GrpcContext.
void register_rpc_handler(asio::io_context& io_context, agrpc::GrpcContext& grpc_context,
                          example::v1::Example::AsyncService& service, grpc::Server& server)
{
    using RPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestUnary>;
    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [&](RPC& rpc, RPC::Request& request) -> asio::awaitable<void>
        {
            // This executes on the io_context thread
            example::v1::Response response;
            response.set_integer(request.integer());
            co_await rpc.finish(response, grpc::Status::OK);
            server.Shutdown();
        },
        // Bind the io_context such that the above rpc handler is invoked on it
        asio::bind_executor(io_context, example::RethrowFirstArg{}));
}

int main(int argc, const char** argv)
{
    const auto grpc_port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + grpc_port;
    const auto tcp_port = static_cast<asio::ip::port_type>(argc >= 3 ? std::stoul(argv[2]) : 8000);

    asio::io_context io_context{1};

    example::v1::Example::AsyncService service;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    register_rpc_handler(io_context, grpc_context, service, *server);
    asio::co_spawn(io_context, handle_tcp_request(tcp_port), example::RethrowFirstArg{});

    std::thread t{[&]
                  {
                      grpc_context.run_completion_queue();
                  }};
    io_context.run();
    t.join();
}