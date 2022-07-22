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

#include "example/v1/example.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <optional>

namespace asio = boost::asio;

// Example showing how to run an io_context and a GrpcContext on the same thread.

// A simple tcp request that will be handled by the io_context
asio::awaitable<void> make_tcp_request(asio::ip::port_type port)
{
    const auto executor = co_await asio::this_coro::executor;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address_v4("127.0.0.1"), port);
    asio::ip::tcp::socket socket(executor);
    co_await socket.async_connect(endpoint, asio::use_awaitable);
    co_await asio::async_write(socket, asio::buffer("example"), asio::use_awaitable);
}

// A unary RPC request that will be handled by the GrpcContext
asio::awaitable<void> make_grpc_request(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
    example::v1::Request request;
    request.set_integer(42);
    const auto reader =
        agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context, request, grpc_context);
    example::v1::Response response;
    grpc::Status status;
    co_await agrpc::finish(reader, response, status, asio::bind_executor(grpc_context, asio::use_awaitable));

    abort_if_not(status.ok());
    abort_if_not(42 == response.integer());
}

int main(int argc, const char** argv)
{
    const auto grpc_port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("localhost:") + grpc_port;
    const auto tcp_port = static_cast<asio::ip::port_type>(argc >= 3 ? std::stoul(argv[2]) : 8000);

    asio::io_context io_context{1};

    const auto channel = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    const auto stub = example::v1::Example::NewStub(channel);
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    std::optional grpc_context_work_guard{
        asio::prefer(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)};

    asio::co_spawn(
        io_context,
        [&]() -> asio::awaitable<void>
        {
            // The two operations below will run concurrently on the same thread.
            using namespace asio::experimental::awaitable_operators;
            co_await (make_grpc_request(grpc_context, *stub) && make_tcp_request(tcp_port));
            grpc_context_work_guard.reset();
        },
        asio::detached);

    // First, initiate the io_context's thread_local variables.
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