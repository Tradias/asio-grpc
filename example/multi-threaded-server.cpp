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

#include "helloworld/helloworld.grpc.pb.h"

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/signal_set.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <forward_list>
#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;

struct ServerShutdown
{
    grpc::Server& server;
    asio::basic_signal_set<agrpc::GrpcContext::executor_type> signals;
    std::atomic_bool is_shutdown{};
    std::thread shutdown_thread;

    ServerShutdown(grpc::Server& server, agrpc::GrpcContext& grpc_context)
        : server(server), signals(grpc_context, SIGINT, SIGTERM)
    {
        signals.async_wait(
            [&](auto&& ec, auto&&)
            {
                if (asio::error::operation_aborted != ec)
                {
                    shutdown();
                }
            });
    }

    void shutdown()
    {
        if (!is_shutdown.exchange(true))
        {
            // We cannot call server.Shutdown() on the same thread that runs a GrpcContext because that could lead to
            // deadlock, therefore create a new thread.
            shutdown_thread = std::thread(
                [&]
                {
                    signals.cancel();
                    server.Shutdown();
                });
        }
    }

    ~ServerShutdown()
    {
        if (shutdown_thread.joinable())
        {
            shutdown_thread.join();
        }
        else if (!is_shutdown.exchange(true))
        {
            server.Shutdown();
        }
    }
};

static std::atomic_int counter{};

void register_request_handler(agrpc::GrpcContext& grpc_context, helloworld::Greeter::AsyncService& service,
                              ServerShutdown& shutdown)
{
    agrpc::repeatedly_request(
        &helloworld::Greeter::AsyncService::RequestSayHello, service,
        asio::bind_executor(
            grpc_context,
            [&](grpc::ServerContext&, helloworld::HelloRequest& request,
                grpc::ServerAsyncResponseWriter<helloworld::HelloReply>& writer) -> asio::awaitable<void>
            {
                helloworld::HelloReply response;
                response.set_message("Hello " + request.name());
                co_await agrpc::finish(writer, response, grpc::Status::OK);

                // In this example we shut down the server after 20 requests
                if (19 == counter.fetch_add(1))
                {
                    shutdown.shutdown();
                }
            }));
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;
    const auto thread_count = std::thread::hardware_concurrency();

    std::unique_ptr<grpc::Server> server;
    helloworld::Greeter::AsyncService service;
    std::forward_list<agrpc::GrpcContext> grpc_contexts;

    {
        grpc::ServerBuilder builder;
        for (size_t i = 0; i < thread_count; ++i)
        {
            grpc_contexts.emplace_front(builder.AddCompletionQueue());
        }
        builder.AddListeningPort(host, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        server = builder.BuildAndStart();
    }

    ServerShutdown shutdown{*server, grpc_contexts.front()};

    // Create one thread per GrpcContext.
    std::vector<std::thread> threads;
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(
            [&, i]
            {
                auto& grpc_context = *std::next(grpc_contexts.begin(), i);
                register_request_handler(grpc_context, service, shutdown);
                grpc_context.run();
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
}