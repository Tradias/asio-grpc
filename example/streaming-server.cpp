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
#include "example/v1/exampleExt.grpc.pb.h"
#include "helper.hpp"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>
#include <optional>
#include <thread>

namespace asio = boost::asio;

// Example showing some of the features of using asio-grpc with Boost.Asio.

// ---------------------------------------------------
// A helper to properly shut down a gRPC without deadlocking.
// ---------------------------------------------------
struct ServerShutdown
{
    grpc::Server& server;
    asio::basic_signal_set<agrpc::GrpcContext::executor_type> signals;
    std::optional<std::thread> shutdown_thread;

    ServerShutdown(grpc::Server& server, agrpc::GrpcContext& grpc_context)
        : server(server), signals(grpc_context, SIGINT, SIGTERM)
    {
        signals.async_wait(
            [&](auto&&, auto&&)
            {
                shutdown();
            });
    }

    void shutdown()
    {
        if (!shutdown_thread)
        {
            // This will cause all coroutines to run to completion normally
            // while returning `false` from RPC related steps, cancelling the signal
            // so that the GrpcContext will eventually run out of work and return
            // from `run()`.
            shutdown_thread.emplace(
                [&]
                {
                    signals.cancel();
                    server.Shutdown();
                });
            // Alternatively call `grpc_context.stop()` here instead which causes all coroutines
            // to end at their next suspension point.
            // Then call `server->Shutdown()` after the call to `grpc_context.run()` returns
            // or `.reset()` the grpc_context and go into another `grpc_context.run()`
        }
    }

    ~ServerShutdown()
    {
        if (shutdown_thread)
        {
            shutdown_thread->join();
        }
    }
};
// ---------------------------------------------------
//

// ---------------------------------------------------
// A simple client-streaming request handler using coroutines.
// ---------------------------------------------------
asio::awaitable<void> handle_client_streaming_request(
    grpc::ServerContext&, grpc::ServerAsyncReader<example::v1::Response, example::v1::Request>& reader)
{
    // Optionally send initial metadata first.
    // Since the default completion token in asio-grpc is asio::use_awaitable, this line is equivalent to:
    // co_await agrpc::send_initial_metadata(reader, asio::use_awaitable);
    bool send_ok = co_await agrpc::send_initial_metadata(reader);

    bool read_ok;
    do
    {
        example::v1::Request request;
        // Read from the client stream until the client has signaled `writes_done`.
        read_ok = co_await agrpc::read(reader, request);
    } while (read_ok);

    example::v1::Response response;
    bool finish_ok = co_await agrpc::finish(reader, response, grpc::Status::OK);

    // Or finish with an error
    // bool finish_with_error_ok = co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);

    // See documentation for the meaning of the bool values

    silence_unused(send_ok, finish_ok);
}

void register_client_streaming_handler(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    // Register a handler for all incoming RPCs of this method (Example::ClientStreaming) until the server is being
    // shut down. An API for requesting to handle a single RPC is also available:
    // `agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service, server_context, reader)`
    agrpc::repeatedly_request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
                              asio::bind_executor(grpc_context, &handle_client_streaming_request));
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The following bidirectional-streaming example shows how to dispatch requests to a thread_pool and write responses
// back to the client.
// ---------------------------------------------------
using Channel = asio::experimental::channel<void(boost::system::error_code, example::v1::Request)>;

// This function will read one requests from the client at a time. Note that gRPC only allows calling agrpc::read after
// a previous read has completed.
asio::awaitable<void> reader(grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request>& reader_writer,
                             Channel& channel)
{
    while (true)
    {
        example::v1::Request request;
        if (!co_await agrpc::read(reader_writer, request))
        {
            // Client is done writing.
            break;
        }
        // Send request to writer. Using detached as completion token since we do not want to wait until the writer
        // has picked up the request.
        channel.async_send(boost::system::error_code{}, std::move(request), asio::detached);
    }
    // Signal the writer to complete.
    channel.close();
}

// The writer will pick up reads from the reader through the channel and switch to the thread_pool to compute their
// response.
asio::awaitable<bool> writer(grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request>& reader_writer,
                             Channel& channel, asio::thread_pool& thread_pool)
{
    bool ok{true};
    while (ok)
    {
        boost::system::error_code ec;
        const auto request = co_await channel.async_receive(asio::redirect_error(asio::use_awaitable, ec));
        if (ec)
        {
            // Channel got closed by the reader.
            break;
        }
        // Switch to the thread_pool.
        co_await asio::post(asio::bind_executor(thread_pool, asio::use_awaitable));
        // Compute the response.
        example::v1::Response response;
        response.set_integer(request.integer() * 2);
        // reader_writer is thread-safe so we can just interact with it from the thread_pool.
        ok = co_await agrpc::write(reader_writer, response);
        // Now we are back on the main thread.
    }
    co_return ok;
}

asio::awaitable<void> handle_bidirectional_streaming_request(example::v1::Example::AsyncService& service,
                                                             asio::thread_pool& thread_pool)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request> reader_writer{&server_context};
    bool request_ok = co_await agrpc::request(&example::v1::Example::AsyncService::RequestBidirectionalStreaming,
                                              service, server_context, reader_writer);
    if (!request_ok)
    {
        // Server is shutting down.
        co_return;
    }
    Channel channel{co_await asio::this_coro::executor};

    using namespace asio::experimental::awaitable_operators;
    const auto ok = co_await (reader(reader_writer, channel) && writer(reader_writer, channel, thread_pool));

    if (!ok)
    {
        // Client has disconnected or server is shutting down.
        co_return;
    }

    bool finish_ok = co_await agrpc::finish(reader_writer, grpc::Status::OK);

    silence_unused(finish_ok);
}

// ---------------------------------------------------
// A bidirectional-streaming RPC where the client subscribes to a topic and the server sends the feed for the last
// subscribed topic every 333ms. The feed is a simple string identified by an integer in the topic.
// ---------------------------------------------------
auto get_feed_for_topic(int32_t id)
{
    example::v1::Feed feed;
    if (0 == id)
    {
        feed.set_content("zero");
    }
    if (1 == id)
    {
        feed.set_content("one");
    }
    if (2 == id)
    {
        feed.set_content("two");
    }
    return feed;
}

asio::awaitable<void> handle_topic_subscription(
    agrpc::GrpcContext& grpc_context, grpc::ServerContext&,
    grpc::ServerAsyncReaderWriter<example::v1::Feed, example::v1::Topic>& reader_writer)
{
    grpc::Alarm alarm;
    agrpc::GrpcStream read_stream{grpc_context};
    agrpc::GrpcStream write_stream{grpc_context};
    example::v1::Topic topic;
    const auto initiate_write = [&]
    {
        if (!write_stream.is_running())
        {
            write_stream.initiate(agrpc::write, reader_writer, get_feed_for_topic(topic.id()));
        }
    };
    bool read_ok = co_await agrpc::read(reader_writer, topic);
    if (read_ok)
    {
        initiate_write();
        read_stream.initiate(agrpc::read, reader_writer, topic);
    }
    std::chrono::system_clock::time_point deadline;
    const auto update_deadline = [&]
    {
        deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(333);
    };
    update_deadline();
    bool write_ok{true};
    do
    {
        using namespace asio::experimental::awaitable_operators;
        const auto variant = co_await (read_stream.next() || agrpc::wait(alarm, deadline) || write_stream.next());
        if (0 == variant.index())  // read completed
        {
            read_ok = std::get<0>(variant);
            if (read_ok)
            {
                read_stream.initiate(agrpc::read, reader_writer, topic);
            }
            update_deadline();
            initiate_write();
        }
        else if (1 == variant.index())  // wait completed
        {
            update_deadline();
            initiate_write();
        }
        else  // write completed
        {
            write_ok = std::get<2>(variant);
        }
    } while (read_ok && write_ok);
    co_await read_stream.cleanup();
    co_await write_stream.cleanup();
    co_await agrpc::finish(reader_writer, grpc::Status::OK);
}

void register_subscription_handler(example::v1::ExampleExt::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    agrpc::repeatedly_request(&example::v1::ExampleExt::AsyncService::RequestSubscribe, service,
                              asio::bind_executor(grpc_context,
                                                  [&](auto&&... args)
                                                  {
                                                      return handle_topic_subscription(
                                                          grpc_context, std::forward<decltype(args)>(args)...);
                                                  }));
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The SlowUnary endpoint is used by the client to demonstrate per-RPC step cancellation. See streaming-client.cpp.
// ---------------------------------------------------
asio::awaitable<void> handle_slow_unary_request(example::v1::ExampleExt::AsyncService& service)
{
    grpc::ServerContext server_context;
    example::v1::SlowRequest request;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    if (!co_await agrpc::request(&example::v1::ExampleExt::AsyncService::RequestSlowUnary, service, server_context,
                                 request, writer))
    {
        co_return;
    }

    grpc::Alarm alarm;
    co_await agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::milliseconds(request.delay()));

    co_await agrpc::finish(writer, {}, grpc::Status::OK);
}
// ---------------------------------------------------
//

// ---------------------------------------------------
// The Shutdown endpoint that is used by unit tests.
// ---------------------------------------------------
asio::awaitable<void> handle_shutdown_request(example::v1::ExampleExt::AsyncService& service,
                                              ServerShutdown& server_shutdown)
{
    grpc::ServerContext server_context;
    grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{&server_context};
    google::protobuf::Empty request;
    if (!co_await agrpc::request(&example::v1::ExampleExt::AsyncService::RequestShutdown, service, server_context,
                                 request, writer))
    {
        co_return;
    }

    google::protobuf::Empty response;
    if (co_await agrpc::finish(writer, response, grpc::Status::OK))
    {
        std::cout << "Received shutdown request from client\n";
        server_shutdown.shutdown();
    }
}
// ---------------------------------------------------
//

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("0.0.0.0:") + port;

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    builder.AddListeningPort(host, grpc::InsecureServerCredentials());
    example::v1::Example::AsyncService service;
    builder.RegisterService(&service);
    example::v1::ExampleExt::AsyncService service_ext;
    builder.RegisterService(&service_ext);
    server = builder.BuildAndStart();
    abort_if_not(bool{server});

    ServerShutdown server_shutdown{*server, grpc_context};

    asio::thread_pool thread_pool{1};

    register_client_streaming_handler(service, grpc_context);
    register_subscription_handler(service_ext, grpc_context);
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await handle_bidirectional_streaming_request(service, thread_pool);
        },
        asio::detached);
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await handle_slow_unary_request(service_ext);
        },
        asio::detached);
    asio::co_spawn(
        grpc_context,
        [&]() -> asio::awaitable<void>
        {
            co_await handle_shutdown_request(service_ext, server_shutdown);
        },
        asio::detached);

    grpc_context.run();
    std::cout << "Shutdown completed\n";
}