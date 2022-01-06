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

#include "helper.hpp"
#include "protos/example.grpc.pb.h"

#include <agrpc/asioGrpc.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/experimental/deferred.hpp>
#include <boost/asio/spawn.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <chrono>

void timer(const boost::asio::yield_context& yield)
{
    // begin-snippet: alarm
    grpc::Alarm alarm;
    bool wait_ok = agrpc::wait(alarm, std::chrono::system_clock::now() + std::chrono::seconds(1), yield);
    // end-snippet

    silence_unused(wait_ok);
}

void timer_with_different_completion_tokens(agrpc::GrpcContext& grpc_context, const boost::asio::yield_context& yield)
{
    grpc::Alarm alarm;
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
    // begin-snippet: alarm-with-callback
    agrpc::wait(alarm, deadline, boost::asio::bind_executor(grpc_context, [&](bool /*wait_ok*/) {}));
    // end-snippet

    // begin-snippet: alarm-stackless-coroutine
    struct Coro : boost::asio::coroutine
    {
        using executor_type = agrpc::GrpcContext::executor_type;

        struct Context
        {
            std::chrono::system_clock::time_point deadline;
            agrpc::GrpcContext& grpc_context;
            grpc::Alarm alarm;

            Context(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context)
                : deadline(deadline), grpc_context(grpc_context)
            {
            }
        };

        std::shared_ptr<Context> context;

        Coro(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context)
            : context(std::make_shared<Context>(deadline, grpc_context))
        {
        }

        void operator()(bool wait_ok)
        {
            BOOST_ASIO_CORO_REENTER(*this)
            {
                BOOST_ASIO_CORO_YIELD agrpc::wait(context->alarm, context->deadline, *this);
                (void)wait_ok;
            }
        }

        executor_type get_executor() const noexcept { return context->grpc_context.get_executor(); }
    };
    Coro{deadline, grpc_context}(false);
    // end-snippet

    // begin-snippet: alarm-double-deferred
    auto deferred_op = agrpc::wait(alarm, deadline,
                                   boost::asio::experimental::deferred(
                                       [&](bool /*wait_ok*/)
                                       {
                                           return agrpc::wait(alarm, deadline + std::chrono::seconds(1),
                                                              boost::asio::experimental::deferred);
                                       }));
    std::move(deferred_op)(yield);
    // end-snippet
}

void unary(example::v1::Example::AsyncService& service, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-unary-server-side
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncResponseWriter<example::v1::Response> writer{&server_context};
    bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestUnary, service, server_context,
                                     request, writer, yield);
    // end-snippet

    // begin-snippet: unary-server-side
    bool send_ok = agrpc::send_initial_metadata(writer, yield);

    example::v1::Response response;
    bool finish_ok = agrpc::finish(writer, response, grpc::Status::OK, yield);

    bool finish_with_error_ok = agrpc::finish_with_error(writer, grpc::Status::CANCELLED, yield);
    // end-snippet

    silence_unused(request_ok, send_ok, finish_ok, finish_with_error_ok);
}

void client_streaming(example::v1::Example::AsyncService& service, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-client-streaming-server-side
    grpc::ServerContext server_context;
    grpc::ServerAsyncReader<example::v1::Response, example::v1::Request> reader{&server_context};
    bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
                                     server_context, reader, yield);
    // end-snippet

    // begin-snippet: client-streaming-server-side
    bool send_ok = agrpc::send_initial_metadata(reader, yield);

    example::v1::Request request;
    bool read_ok = agrpc::read(reader, request, yield);

    example::v1::Response response;
    bool finish_ok = agrpc::finish(reader, response, grpc::Status::OK, yield);

    bool finish_with_error_ok = agrpc::finish_with_error(reader, grpc::Status::CANCELLED, yield);
    // end-snippet

    silence_unused(request_ok, send_ok, read_ok, finish_with_error_ok, finish_ok);
}

void server_streaming(example::v1::Example::AsyncService& service, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-server-streaming-server-side
    grpc::ServerContext server_context;
    example::v1::Request request;
    grpc::ServerAsyncWriter<example::v1::Response> writer{&server_context};
    bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestServerStreaming, service,
                                     server_context, request, writer, yield);
    // end-snippet

    // begin-snippet: server-streaming-server-side
    bool send_ok = agrpc::send_initial_metadata(writer, yield);

    example::v1::Response response;
    bool write_ok = agrpc::write(writer, response, yield);

    bool write_and_finish_ok = agrpc::write_and_finish(writer, response, grpc::WriteOptions{}, grpc::Status::OK, yield);

    bool finish_ok = agrpc::finish(writer, grpc::Status::OK, yield);
    // end-snippet

    silence_unused(request_ok, send_ok, write_ok, write_and_finish_ok, finish_ok);
}

void bidirectional_streaming(example::v1::Example::AsyncService& service, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-bidirectional-streaming-server-side
    grpc::ServerContext server_context;
    grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request> reader_writer{&server_context};
    bool request_ok = agrpc::request(&example::v1::Example::AsyncService::RequestBidirectionalStreaming, service,
                                     server_context, reader_writer, yield);
    // end-snippet

    // begin-snippet: bidirectional-streaming-server-side
    bool send_ok = agrpc::send_initial_metadata(reader_writer, yield);

    example::v1::Request request;
    bool read_ok = agrpc::read(reader_writer, request, yield);

    example::v1::Response response;
    bool write_and_finish_ok =
        agrpc::write_and_finish(reader_writer, response, grpc::WriteOptions{}, grpc::Status::OK, yield);

    bool write_ok = agrpc::write(reader_writer, response, yield);

    bool finish_ok = agrpc::finish(reader_writer, grpc::Status::OK, yield);
    // end-snippet

    silence_unused(request_ok, send_ok, read_ok, write_and_finish_ok, write_ok, finish_ok);
}

// begin-snippet: repeatedly-request-spawner
template <class Handler>
struct Spawner
{
    using executor_type = boost::asio::associated_executor_t<Handler>;
    using allocator_type = boost::asio::associated_allocator_t<Handler>;

    Handler handler;

    explicit Spawner(Handler handler) : handler(std::move(handler)) {}

    template <class T>
    void operator()(agrpc::RepeatedlyRequestContext<T>&& request_context, bool request_ok) &&
    {
        if (!request_ok)
        {
            return;
        }
        auto executor = this->get_executor();
        boost::asio::spawn(
            std::move(executor),
            [handler = std::move(handler),
             request_context = std::move(request_context)](const boost::asio::yield_context& yield) mutable
            {
                std::apply(std::move(handler), std::tuple_cat(request_context.args(), std::forward_as_tuple(yield)));
                // Or
                // std::invoke(std::move(request_context), std::move(handler), yield);
                //
                // The RepeatedlyRequestContext also provides access to:
                // * the grpc::ServerContext
                // request_context.server_context();
                // * the grpc::ServerAsyncReader/Writer
                // request_context.responder();
                // * the protobuf request message (for unary and server-streaming requests)
                // request_context.request();
            });
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return boost::asio::get_associated_executor(handler); }

    [[nodiscard]] allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(handler);
    }
};

void repeatedly_request_example(example::v1::Example::AsyncService& service, agrpc::GrpcContext& grpc_context)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        Spawner{boost::asio::bind_executor(
            grpc_context,
            [&](grpc::ServerContext&, example::v1::Request&,
                grpc::ServerAsyncResponseWriter<example::v1::Response> writer, const boost::asio::yield_context& yield)
            {
                example::v1::Response response;
                agrpc::finish(writer, response, grpc::Status::OK, yield);
            })});
}
// end-snippet

int main()
{
    std::unique_ptr<grpc::Server> server;
    example::v1::Example::AsyncService service;

    // begin-snippet: create-grpc_context-server-side
    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
    // end-snippet

    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = builder.BuildAndStart();

    auto guard = boost::asio::make_work_guard(grpc_context);
    boost::asio::spawn(grpc_context,
                       [&](boost::asio::yield_context yield)
                       {
                           unary(service, yield);
                       });

    // begin-snippet: run-grpc_context-server-side
    grpc_context.run();
    server->Shutdown();
}  // grpc_context is destructed here before the server
   // end-snippet