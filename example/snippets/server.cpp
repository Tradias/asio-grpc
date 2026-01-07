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

#include "example/v1/example.grpc.pb.h"
#include "grpc/health/v1/health.grpc.pb.h"
#include "helper.hpp"
#include "server_shutdown_asio.hpp"

#include <agrpc/asio_grpc.hpp>
#include <agrpc/health_check_service.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/use_promise.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <optional>

namespace asio = boost::asio;

void create_server_grpc_context()
{
    /* [create-grpc_context-server-side] */
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    /* [create-grpc_context-server-side] */
}

void create_multi_threaded_server_grpc_context()
{
    /* [create-multi-threaded-grpc_context-server-side] */
    const auto concurrency = std::thread::hardware_concurrency();
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue(), concurrency};
    // ... register services, start server
    std::vector<std::thread> threads(concurrency);
    for (auto& thread : threads)
    {
        thread = std::thread{[&]
                             {
                                 // ... register rpc handlers
                                 grpc_context.run();
                             }};
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    /* [create-multi-threaded-grpc_context-server-side] */
}

void health_check_service()
{
    /* [add-health-check-service] */
    std::unique_ptr<grpc::Server> server;
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    agrpc::add_health_check_service(builder);
    server = builder.BuildAndStart();
    agrpc::start_health_check_service(*server, grpc_context);
    /* [add-health-check-service] */
}

void server_main()
{
    example::v1::Example::AsyncService service;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};

    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    std::optional guard{asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)};
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_return;
        },
        asio::detached);

    grpc_context.run();
    server->Shutdown();
}

void register_handlers(agrpc::GrpcContext&, example::v1::Example::AsyncService&) {}

void server_main_cheat_sheet()
{
    /* [server-main-cheat-sheet] */
    example::v1::Example::AsyncService service;
    std::unique_ptr<grpc::Server> server;
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();
    example::ServerShutdown shutdown{*server, grpc_context};
    register_handlers(grpc_context, service);
    grpc_context.run();
    /* [server-main-cheat-sheet] */
}