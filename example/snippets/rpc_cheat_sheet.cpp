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

#include <agrpc/repeatedly_request.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/wait.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;

// Explicitly formatted using `ColumnLimit: 90`
// clang-format off
/* [agrpc-wait] */
asio::awaitable<void> agrpc_wait()
{
    grpc::Alarm alarm;
    // implicitly uses GrpcContext stored in asio::awaitable:
    bool wait_ok = co_await agrpc::wait(
        alarm, std::chrono::system_clock::now() + std::chrono::seconds(1));
    (void)wait_ok;
}
/* [agrpc-wait] */

/* [full-unary-client-side] */
asio::awaitable<void> unary_rpc(agrpc::GrpcContext& grpc_context,
                                example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() +
                                std::chrono::seconds(5));
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        agrpc::request(&example::v1::Example::Stub::AsyncUnary, stub, client_context,
                       request, grpc_context);
    example::v1::Response response;
    grpc::Status status;
    co_await agrpc::finish(reader, response, status);
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
    std::cout << "Response: " << response.integer();
}
/* [full-unary-client-side] */

/* [full-client-streaming-client-side] */
asio::awaitable<void> client_streaming_rpc(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() +
                                std::chrono::seconds(5));

    example::v1::Response response;

    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;
    if (!co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncClientStreaming,
                                 stub, client_context, writer, response))
    {
        grpc::Status status;
        co_await agrpc::finish(writer, status);
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    example::v1::Request request;
    request.set_integer(1);
    while (co_await agrpc::write(writer, request) && request.integer() < 42)
    {
        request.set_integer(request.integer() + 1);
    }
    co_await agrpc::writes_done(writer);

    grpc::Status status;
    co_await agrpc::finish(writer, status);
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    std::cout << "Response: " << response.integer();
}
/* [full-client-streaming-client-side] */

/* [full-server-streaming-client-side] */
asio::awaitable<void> server_streaming_rpc(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() +
                                std::chrono::seconds(5));

    example::v1::Request request;
    request.set_integer(42);
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    if (!co_await agrpc::request(&example::v1::Example::Stub::PrepareAsyncServerStreaming,
                                 stub, client_context, request, reader))
    {
        grpc::Status status;
        co_await agrpc::finish(reader, status);
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    example::v1::Response response;
    while (co_await agrpc::read(reader, response))
    {
        std::cout << "Response: " << response.integer() << '\n';
    }

    grpc::Status status;
    co_await agrpc::finish(reader, status);
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
}
/* [full-server-streaming-client-side] */

/* [full-bidirectional-client-side] */
asio::awaitable<void> bidirectional_streaming_rpc(example::v1::Example::Stub& stub)
{
    grpc::ClientContext client_context;
    client_context.set_deadline(std::chrono::system_clock::now() +
                                std::chrono::seconds(5));

    std::unique_ptr<
        grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>>
        reader_writer;
    if (!co_await agrpc::request(
            &example::v1::Example::Stub::PrepareAsyncBidirectionalStreaming, stub,
            client_context, reader_writer))
    {
        grpc::Status status;
        co_await agrpc::finish(reader_writer, status);
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }

    example::v1::Request request;

    bool write_ok{true};
    example::v1::Response response;
    while (co_await agrpc::read(reader_writer, response) && write_ok)
    {
        request.set_integer(response.integer() + 1);
        write_ok = co_await agrpc::write(reader_writer, request);
    }
    co_await agrpc::writes_done(reader_writer);

    grpc::Status status;
    co_await agrpc::finish(reader_writer, status);
    if (!status.ok())
    {
        std::cerr << "Rpc failed: " << status.error_message();
        co_return;
    }
}
/* [full-bidirectional-client-side] */

// server
/* [full-unary-server-side] */
void unary_rpc(agrpc::GrpcContext& grpc_context,
               example::v1::Example::AsyncService& service)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestUnary, service,
        asio::bind_executor(
            grpc_context,
            [](grpc::ServerContext& /*server_context*/, example::v1::Request& /*request*/,
               grpc::ServerAsyncResponseWriter<example::v1::Response>& writer)
                -> asio::awaitable<void>
            {
                example::v1::Response response;
                co_await agrpc::finish(writer, response, grpc::Status::OK);

                // Alternatively finish with an error.
                co_await agrpc::finish_with_error(writer, grpc::Status::CANCELLED);
            }));
}
/* [full-unary-server-side] */

/* [full-client-streaming-server-side] */
void client_streaming_rpc(agrpc::GrpcContext& grpc_context,
                          example::v1::Example::AsyncService& service)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestClientStreaming, service,
        asio::bind_executor(
            grpc_context,
            [](grpc::ServerContext& /*server_context*/,
               grpc::ServerAsyncReader<example::v1::Response, example::v1::Request>&
                   reader) -> asio::awaitable<void>
            {
                example::v1::Request request;
                while (co_await agrpc::read(reader, request))
                {
                    std::cout << "Request: " << request.integer() << std::endl;
                }
                example::v1::Response response;
                response.set_integer(42);
                co_await agrpc::finish(reader, response, grpc::Status::OK);

                // Alternatively finish with an error.
                co_await agrpc::finish_with_error(reader, grpc::Status::CANCELLED);
            }));
}
/* [full-client-streaming-server-side] */

/* [full-server-streaming-server-side] */
void server_streaming_rpc(agrpc::GrpcContext& grpc_context,
                          example::v1::Example::AsyncService& service)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestServerStreaming, service,
        asio::bind_executor(
            grpc_context,
            [](grpc::ServerContext& /*server_context*/, example::v1::Request& request,
               grpc::ServerAsyncWriter<example::v1::Response>& writer)
                -> asio::awaitable<void>
            {
                example::v1::Response response;
                for (int i{}; i != request.integer(); ++i)
                {
                    response.set_integer(i);
                    if (!co_await agrpc::write(writer, response))
                    {
                        co_return;
                    }
                }
                co_await agrpc::finish(writer, grpc::Status::OK);
            }));
}
/* [full-server-streaming-server-side] */

/* [full-bidirectional-streaming-server-side] */
void bidirectional_streaming_rpc(agrpc::GrpcContext& grpc_context,
                                 example::v1::Example::AsyncService& service)
{
    agrpc::repeatedly_request(
        &example::v1::Example::AsyncService::RequestBidirectionalStreaming, service,
        asio::bind_executor(
            grpc_context,
            [](grpc::ServerContext& /*server_context*/,
               grpc::ServerAsyncReaderWriter<example::v1::Response, example::v1::Request>&
                   reader_writer) -> asio::awaitable<void>
            {
                example::v1::Request request;
                example::v1::Response response;
                while (co_await agrpc::read(reader_writer, request))
                {
                    response.set_integer(request.integer());
                    if (!co_await agrpc::write(reader_writer, response))
                    {
                        co_return;
                    }
                }
                response.set_integer(42);
                co_await agrpc::write_last(reader_writer, response, grpc::WriteOptions{});
                co_await agrpc::finish(reader_writer, grpc::Status::OK);
            }));
}
/* [full-bidirectional-streaming-server-side] */
// clang-format on
