// Copyright 2023 Dennis Hezel
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
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "server_shutdown_asio.hpp"
#include "yield_helper.hpp"

#include <agrpc/asio_grpc.hpp>
#include <agrpc/register_yield_rpc_handler.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>
#include <memory>
#include <thread>

namespace asio = boost::asio;

// Examples showing how to write generic servers for unary and bidirectional streaming RPCs.

// begin-snippet: server-side-generic-unary-request
// ---------------------------------------------------
// Handle a simple generic unary request with Boost.Coroutine.
// ---------------------------------------------------
// end-snippet
void process_request(grpc::ByteBuffer& buffer)
{
    // -- Deserialize the request message
    example::v1::Request request;
    const auto deserialize_status =
        grpc::GenericDeserialize<grpc::ProtoBufferReader, example::v1::Request>(&buffer, &request);

    abort_if_not(deserialize_status.ok());

    // -- Serialize the response message
    example::v1::Response response;
    response.set_integer(request.integer() * 2);
    bool own_buffer;
    grpc::GenericSerialize<grpc::ProtoBufferWriter, example::v1::Response>(response, &buffer, &own_buffer);
}

void handle_generic_unary_request(agrpc::GenericServerRPC& rpc, const asio::yield_context& yield)
{
    grpc::ByteBuffer buffer;

    // -- Wait for the request message
    rpc.read(buffer, yield);

    process_request(buffer);

    // -- Write the response message and finish this RPC with OK
    rpc.write_and_finish(buffer, {}, grpc::Status::OK, yield);
}
// ---------------------------------------------------
//

// begin-snippet: server-side-generic-bidirectional-request
// ---------------------------------------------------
// A bidirectional-streaming example that shows how to dispatch requests to a thread_pool and write responses
// back to the client.
// ---------------------------------------------------
// end-snippet
using Channel = asio::experimental::channel<agrpc::GrpcExecutor, void(boost::system::error_code, grpc::ByteBuffer)>;

template <class Handler>
void reader(agrpc::GenericServerRPC& rpc, Channel& channel, const asio::basic_yield_context<Handler>& yield)
{
    while (true)
    {
        grpc::ByteBuffer buffer;
        if (!rpc.read(buffer, yield))
        {
            std::cout << "Generic: Client is done writing." << std::endl;
            break;
        }
        // Send request to writer. The `max_buffer_size` of the channel acts as backpressure.
        boost::system::error_code ec;
        channel.async_send(boost::system::error_code{}, std::move(buffer), yield[ec]);
    }
    // Signal the writer to complete.
    channel.close();
}

// When switching threads in a Boost.Coroutine calls to `std::this_thread::get_id` before and after the switch can
// produce unexpected results. Disabling optimizations seems to correct that.
auto
#if defined(__clang__)
    __attribute__((optnone))
#elif defined(__GNUC__)
    __attribute__((optimize("O0")))
#endif
    get_thread_id()
{
    return std::this_thread::get_id();
}

// The writer will pick up reads from the reader through the channel and switch
// to the thread_pool to compute their response.
template <class Handler>
bool writer(agrpc::GenericServerRPC& rpc, Channel& channel, asio::thread_pool& thread_pool,
            const asio::basic_yield_context<Handler>& yield)
{
    bool ok{true};
    while (ok)
    {
        boost::system::error_code ec;
        auto buffer = channel.async_receive(yield[ec]);
        if (ec)
        {
            break;
        }
        auto main_thread = std::this_thread::get_id();

        // In this example we switch to the thread_pool to compute the response.
        asio::post(asio::bind_executor(thread_pool, yield));

        auto thread_pool_thread = get_thread_id();
        abort_if_not(main_thread != thread_pool_thread);

        process_request(buffer);

        // rpc.write() is thread-safe so we can interact with it from the thread_pool.
        ok = rpc.write(buffer, yield);
        // Now we are back on the main thread.
    }
    std::cout << "Generic: Server writes completed with: " << std::boolalpha << ok << std::endl;
    return ok;
}

void handle_generic_bidistream_request(agrpc::GrpcContext& grpc_context, agrpc::GenericServerRPC& rpc,
                                       asio::thread_pool& thread_pool, const asio::yield_context& yield)
{
    // Maximum number of requests that are buffered by the channel to enable backpressure.
    static constexpr auto MAX_BUFFER_SIZE = 2;

    Channel channel{grpc_context, MAX_BUFFER_SIZE};

    bool ok{};

    example::spawn_all_void(
        grpc_context, yield,
        [&](const auto& yield)
        {
            reader(rpc, channel, yield);
        },
        [&](const auto& yield)
        {
            ok = writer(rpc, channel, thread_pool, yield);
        });

    if (!ok)
    {
        std::cout << "Client has disconnected or server is shutting down." << std::endl;
        return;
    }

    rpc.finish(grpc::Status::OK, yield);
}
// ---------------------------------------------------
//

struct GenericRequestHandler
{
    using executor_type = agrpc::GrpcContext::executor_type;

    agrpc::GrpcContext& grpc_context;
    asio::thread_pool& thread_pool;

    void operator()(agrpc::GenericServerRPC& rpc, const asio::yield_context& yield) const
    {
        const auto& method = rpc.context().method();
        if ("/example.v1.Example/Unary" == method)
        {
            handle_generic_unary_request(rpc, yield);
        }
        else if ("/example.v1.Example/BidirectionalStreaming" == method)
        {
            handle_generic_bidistream_request(grpc_context, rpc, thread_pool, yield);
        }
        else
        {
            throw std::runtime_error("Unsupport method!");
        }
    }

    auto get_executor() const noexcept { return grpc_context.get_executor(); }
};

using ShutdownService = example::v1::ExampleExt::WithAsyncMethod_Shutdown<example::v1::ExampleExt::Service>;
using ShutdownRPC = agrpc::ServerRPC<&ShutdownService::RequestShutdown>;

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    grpc::AsyncGenericService service;
    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());

    builder.RegisterAsyncGenericService(&service);

    // All requests will be handled in a generic fashion except the shutdown request:
    ShutdownService shutdown_service;
    builder.RegisterService(&shutdown_service);

    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    example::ServerShutdown server_shutdown{*server, grpc_context};

    agrpc::register_yield_rpc_handler<ShutdownRPC>(
        grpc_context, shutdown_service,
        [&](ShutdownRPC& rpc, const ShutdownRPC::Request&, const asio::yield_context& yield)
        {
            if (rpc.finish({}, grpc::Status::OK, yield))
            {
                std::cout << "Received shutdown request from client\n";
                server_shutdown.shutdown();
            }
        },
        example::RethrowFirstArg{});

    asio::thread_pool thread_pool{1};
    agrpc::register_yield_rpc_handler<agrpc::GenericServerRPC>(
        grpc_context, service, GenericRequestHandler{grpc_context, thread_pool}, example::RethrowFirstArg{});

    grpc_context.run();
}