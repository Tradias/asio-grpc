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

#include "coro_traits.hpp"
#include "example/v1/example.grpc.pb.h"
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "rethrow_first_arg.hpp"
#include "server_shutdown_asio.hpp"

#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <agrpc/register_coroutine_rpc_handler.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

namespace asio = boost::asio;

using ExampleService = example::v1::Example::AsyncService;
using ExampleExtService = example::v1::ExampleExt::AsyncService;

// begin-snippet: server-side-server-streaming-async-generator

// (experimental) Server handling a server-streaming request using co_yield

// end-snippet

template <class RPCHandler>
struct AsyncGeneratorServerStreamingRPCHandler
{
    explicit AsyncGeneratorServerStreamingRPCHandler(RPCHandler handler) : handler_(std::move(handler)) {}

    template <class ServerRPC>
    asio::experimental::coro<> operator()(agrpc::GrpcExecutor executor, ServerRPC& rpc,
                                          typename ServerRPC::Request& request)
    {
        auto generator = handler_(executor, request);
        while (true)
        {
            const auto response = co_await generator;
            if (response.index() == 0)
            {
                if (!co_await rpc.write(*std::get<0>(response)))
                {
                    co_return;
                }
            }
            else
            {
                co_await rpc.finish(std::get<1>(response));
                co_return;
            }
        }
    }

    RPCHandler handler_;
};

template <class Response>
using ServerStreamingAsyncGeneratorT = asio::experimental::coro<const Response*, grpc::Status>;

// The actual server-streaming rpc handler
// Note how this function knows nothing about agrpc::ServerRPC
ServerStreamingAsyncGeneratorT<example::v1::Response> handle_server_streaming_request(agrpc::GrpcExecutor executor,
                                                                                      example::v1::Request& request)
{
    example::v1::Response response;
    response.set_integer(request.integer());
    agrpc::Alarm alarm{executor};
    for (size_t i{}; i != 5; ++i)
    {
        response.set_integer(response.integer() + 1);
        co_yield &response;
        co_await alarm.wait(std::chrono::system_clock::now() + std::chrono::milliseconds(100));
    }
    co_return grpc::Status::OK;
}

int main(int argc, const char** argv)
{
    const char* port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    ExampleService service;
    builder.RegisterService(&service);
    ExampleExtService service_ext;
    builder.RegisterService(&service_ext);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    example::ServerShutdown server_shutdown{*server, grpc_context};

    agrpc::register_coroutine_rpc_handler<agrpc::ServerRPC<&ExampleService::RequestServerStreaming>,
                                          example::AsioCoroTraits<>>(
        grpc_context, service, AsyncGeneratorServerStreamingRPCHandler{&handle_server_streaming_request},
        example::RethrowFirstArg{});

    agrpc::register_awaitable_rpc_handler<agrpc::ServerRPC<&ExampleExtService::RequestShutdown>>(
        grpc_context, service_ext,
        [&](auto& rpc, const auto&) -> asio::awaitable<void>
        {
            co_await rpc.finish({}, grpc::Status::OK);
            server_shutdown.shutdown();
        },
        example::RethrowFirstArg{});

    grpc_context.run();
}