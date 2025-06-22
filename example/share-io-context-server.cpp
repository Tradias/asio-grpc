// Copyright 2025 Dennis Hezel
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

#include "example/v1/example.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"
#include "server_shutdown_asio.hpp"

#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <agrpc/run.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <string_view>

namespace asio = boost::asio;

// begin-snippet: server-side-share-io-context

// Example showing how to run an io_context and a GrpcContext on the same thread for gRPC servers.
// This can i.e. be useful when writing an HTTP server that occasionally reaches out to a gRPC server. In that case
// creating a separate thread for the GrpcContext might be undesirable due to added synchronization complexity.

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
using RPC = agrpc::ServerRPC<&example::v1::Example::AsyncService::RequestUnary>;

int main(int argc, const char** argv)
{
    const char* grpc_port = argc >= 2 ? argv[1] : "50051";
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

    example::ServerShutdown server_shutdown{*server, grpc_context};

    agrpc::register_awaitable_rpc_handler<RPC>(
        grpc_context, service,
        [&](RPC& rpc, RPC::Request& request) -> asio::awaitable<void>
        {
            example::v1::Response response;
            response.set_integer(request.integer());
            co_await rpc.finish(response, grpc::Status::OK);
            server_shutdown.shutdown();
        },
        example::RethrowFirstArg{});

    asio::co_spawn(io_context, handle_tcp_request(tcp_port), example::RethrowFirstArg{});

    // First, initiate the io_context's thread_local variables by posting on it. The io_context uses them to optimize
    // dynamic memory allocations. This is an optional step but it can improve performance.
    // Then undo the work counting of asio::post.
    // Run GrpcContext and io_context until both stop.
    // Finally, redo the work counting.
    asio::post(io_context,
               [&]
               {
                   io_context.get_executor().on_work_finished();
                   agrpc::run(grpc_context, io_context);
                   io_context.get_executor().on_work_started();
               });
    io_context.run();
}