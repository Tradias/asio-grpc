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
#include "example/v1/example_ext.grpc.pb.h"
#include "helper.hpp"
#include "yield_helper.hpp"

#include <agrpc/asio_grpc.hpp>
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
#include <optional>
#include <thread>

namespace asio = boost::asio;

// Example showing how to write to generic server that handles a single unary request.

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

void handle_generic_unary_request(grpc::GenericServerAsyncReaderWriter& reader_writer, const asio::yield_context& yield)
{
    grpc::ByteBuffer buffer;

    // -- Wait for the request message
    agrpc::read(reader_writer, buffer, yield);

    process_request(buffer);

    // -- Write the response message and finish this RPC with OK
    agrpc::write_and_finish(reader_writer, buffer, {}, grpc::Status::OK, yield);
}

using Channel = asio::experimental::channel<agrpc::GrpcExecutor, void(boost::system::error_code, grpc::ByteBuffer)>;

template <class Handler>
void reader(grpc::GenericServerAsyncReaderWriter& reader_writer, Channel& channel,
            const asio::basic_yield_context<Handler>& yield)
{
    while (true)
    {
        grpc::ByteBuffer buffer;

        if (!agrpc::read(reader_writer, buffer, yield))
        {
            std::cout << "Generic: Client is done writing." << std::endl;
            break;
        }
        // Send request to writer. Using detached as completion token since we do
        // not want to wait until the writer has picked up the request.
        channel.async_send(boost::system::error_code{}, std::move(buffer), asio::detached);
    }
    // Signal the writer to complete.
    channel.close();
}

// GCC and Clang optimize around the fact that a normal function cannot suddendly switch to a different thread and
// they merge the two calls to `std::this_thread::get_id` in the `writer` below.
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
bool writer(grpc::GenericServerAsyncReaderWriter& reader_writer, Channel& channel, asio::thread_pool& thread_pool,
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

        // Switch to the thread_pool.
        asio::post(asio::bind_executor(thread_pool, yield));

        auto thread_pool_thread = get_thread_id();
        abort_if_not(main_thread != thread_pool_thread);

        // Compute the response.
        process_request(buffer);

        // reader_writer is thread-safe so we can just interact with it from the thread_pool.
        ok = agrpc::write(reader_writer, buffer, yield);
        // Now we are back on the main thread.
    }
    std::cout << "Generic: Server writes completed with: " << std::boolalpha << ok << std::endl;
    return ok;
}

void handle_generic_bidistream_request(agrpc::GrpcContext& grpc_context,
                                       grpc::GenericServerAsyncReaderWriter& reader_writer,
                                       asio::thread_pool& thread_pool, const asio::yield_context& yield)
{
    Channel channel{grpc_context};

    bool ok{};

    example::yield_spawn_all(
        grpc_context, yield,
        [&](const auto& yield)
        {
            reader(reader_writer, channel, yield);
        },
        [&](const auto& yield)
        {
            ok = writer(reader_writer, channel, thread_pool, yield);
        });

    if (!ok)
    {
        std::cout << "Client has disconnected or server is shutting down." << std::endl;
        return;
    }

    agrpc::finish(reader_writer, grpc::Status::OK, yield);
}

struct GenericRequestHandler
{
    using executor_type = agrpc::GrpcContext::executor_type;

    agrpc::GrpcContext& grpc_context;
    asio::thread_pool& thread_pool;

    void operator()(agrpc::GenericRepeatedlyRequestContext<>&& context) const
    {
        asio::spawn(grpc_context,
                    [&, context = std::move(context)](asio::yield_context yield)
                    {
                        const auto& method = context.server_context().method();
                        if ("/example.v1.Example/Unary" == method)
                        {
                            handle_generic_unary_request(context.responder(), yield);
                        }
                        else if ("/example.v1.Example/BidirectionalStreaming" == method)
                        {
                            handle_generic_bidistream_request(grpc_context, context.responder(), thread_pool, yield);
                        }
                        else
                        {
                            throw std::runtime_error("Unsupport method!");
                        }
                    });
    }

    auto get_executor() const noexcept { return grpc_context.get_executor(); }
};

using ShutdownService = example::v1::ExampleExt::WithAsyncMethod_Shutdown<example::v1::ExampleExt::Service>;

void handle_shutdown_request(ShutdownService& shutdown_service, grpc::Server& server,
                             std::optional<std::thread>& shutdown_thread, const asio::yield_context& yield)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    google::protobuf::Empty request;
    agrpc::request(&ShutdownService::RequestShutdown, shutdown_service, server_context, request, writer, yield);
    agrpc::finish(writer, {}, grpc::Status::OK, yield);
    shutdown_thread.emplace(
        [&]
        {
            server.Shutdown();
        });
}

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;
    std::optional<std::thread> shutdown_thread;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());

    grpc::AsyncGenericService service;
    builder.RegisterAsyncGenericService(&service);

    // All requests will be handled in a generic fashion except the shutdown request:
    ShutdownService shutdown_service;
    builder.RegisterService(&shutdown_service);

    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    asio::spawn(grpc_context,
                [&](asio::yield_context yield)
                {
                    handle_shutdown_request(shutdown_service, *server, shutdown_thread, yield);
                });

    asio::thread_pool thread_pool{1};
    agrpc::repeatedly_request(service, GenericRequestHandler{grpc_context, thread_pool});

    grpc_context.run();
    if (shutdown_thread && shutdown_thread->joinable())
    {
        shutdown_thread->join();
    }
}