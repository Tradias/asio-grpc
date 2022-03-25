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

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <string_view>

namespace asio = boost::asio;

asio::awaitable<void> handle_tcp_request(asio::ip::port_type port)
{
    const auto executor = co_await asio::this_coro::executor;
    asio::ip::tcp::acceptor acceptor(executor, {asio::ip::make_address_v4("127.0.0.1"), port});
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
    char data[128];
    const auto bytes_read = co_await socket.async_read_some(asio::buffer(data), asio::use_awaitable);

    abort_if_not("example" == std::string_view(data, bytes_read - 1));
}

asio::awaitable<void> handle_grpc_request(agrpc::GrpcContext& grpc_context, example::v1::Example::AsyncService& service)
{
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
    if (!co_await agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service, server_context, request,
                                 writer, asio::bind_executor(grpc_context, asio::use_awaitable)))
    {
        co_return;
    }
    example::v1::Response response;
    response.set_integer(request.integer());
    co_await agrpc::finish(writer, response, grpc::Status::OK, asio::bind_executor(grpc_context, asio::use_awaitable));
}

int main(int argc, const char** argv)
{
    const auto grpc_port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + grpc_port;
    const auto tcp_port = static_cast<asio::ip::port_type>(argc >= 3 ? std::stoul(argv[2]) : 8000);

    asio::io_context io_context{1};

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    example::v1::Example::AsyncService service;
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    agrpc::PollContext poll_context{io_context.get_executor()};
    // Poll the GrpcContext until the io_context stops (runs out of work).
    poll_context.async_poll(grpc_context,
                            [&](auto&&)
                            {
                                if (io_context.stopped())
                                {
                                    // Undo the discount.
                                    io_context.get_executor().on_work_started();
                                    return true;
                                }
                                return false;
                            });

    asio::co_spawn(
        io_context,
        [&]() -> asio::awaitable<void>
        {
            // The two operations below will run concurrently on the same thread.
            using namespace boost::asio::experimental::awaitable_operators;
            co_await(handle_grpc_request(grpc_context, service) && handle_tcp_request(tcp_port));
        },
        asio::detached);

    // Discount the work performed by poll_context.async_poll.
    io_context.get_executor().on_work_finished();

    io_context.run();

    server->Shutdown();
}