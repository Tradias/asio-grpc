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
#include <boost/asio/spawn.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

void unary(agrpc::GrpcContext& grpc_context, example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-unary-client-side
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<example::v1::Response>> reader =
        stub.AsyncUnary(&client_context, request, agrpc::get_completion_queue(grpc_context));
    // end-snippet
    // begin-snippet: unary-client-side
    bool read_ok = agrpc::read_initial_metadata(*reader, yield);

    example::v1::Response response;
    grpc::Status status;
    bool finish_ok = agrpc::finish(*reader, response, status, yield);
    // end-snippet

    silence_unused(read_ok, finish_ok);
}

void request_client_streaming_alt(example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    grpc::ClientContext client_context;
    example::v1::Response response;
    // begin-snippet: request-client-streaming-client-side-alt
    auto [writer, request_ok] =
        agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context, response, yield);
    // end-snippet

    silence_unused(writer, request_ok);
}

void client_streaming(example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-client-streaming-client-side
    grpc::ClientContext client_context;
    example::v1::Response response;
    std::unique_ptr<grpc::ClientAsyncWriter<example::v1::Request>> writer;
    bool request_ok = agrpc::request(&example::v1::Example::Stub::AsyncClientStreaming, stub, client_context, writer,
                                     response, yield);
    // end-snippet

    // begin-snippet: client-streaming-client-side
    bool read_ok = agrpc::read_initial_metadata(*writer, yield);

    example::v1::Request request;
    bool write_ok = agrpc::write(*writer, request, yield);

    bool writes_done_ok = agrpc::writes_done(*writer, yield);

    grpc::Status status;
    bool finish_ok = agrpc::finish(*writer, status, yield);
    // end-snippet

    silence_unused(request_ok, read_ok, write_ok, writes_done_ok, finish_ok);
}

void request_server_streaming_alt(example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    grpc::ClientContext client_context;
    example::v1::Request request;
    // begin-snippet: request-server-streaming-client-side-alt
    auto [reader, request_ok] =
        agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, yield);
    // end-snippet

    silence_unused(reader, request_ok);
}

void server_streaming(example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-server-streaming-client-side
    grpc::ClientContext client_context;
    example::v1::Request request;
    std::unique_ptr<grpc::ClientAsyncReader<example::v1::Response>> reader;
    bool request_ok =
        agrpc::request(&example::v1::Example::Stub::AsyncServerStreaming, stub, client_context, request, reader, yield);
    // end-snippet

    // begin-snippet: server-streaming-client-side
    bool read_metadata_ok = agrpc::read_initial_metadata(*reader, yield);

    example::v1::Response response;
    bool read_ok = agrpc::read(*reader, response, yield);

    grpc::Status status;
    bool finish_ok = agrpc::finish(*reader, status, yield);
    // end-snippet

    silence_unused(request_ok, read_metadata_ok, read_ok, finish_ok);
}

void request_bidirectional_alt(example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    grpc::ClientContext client_context;
    // begin-snippet: request-bidirectional-client-side-alt
    auto [reader_writer, request_ok] =
        agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub, client_context, yield);
    // end-snippet

    silence_unused(reader_writer, request_ok);
}

void bidirectional_streaming(example::v1::Example::Stub& stub, const boost::asio::yield_context& yield)
{
    // begin-snippet: request-bidirectional-client-side
    grpc::ClientContext client_context;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<example::v1::Request, example::v1::Response>> reader_writer;
    bool request_ok = agrpc::request(&example::v1::Example::Stub::AsyncBidirectionalStreaming, stub, client_context,
                                     reader_writer, yield);
    // end-snippet

    // begin-snippet: bidirectional-client-side
    bool read_metadata_ok = agrpc::read_initial_metadata(*reader_writer, yield);

    example::v1::Request request;
    bool write_ok = agrpc::write(*reader_writer, request, yield);

    bool writes_done_ok = agrpc::writes_done(*reader_writer, yield);

    example::v1::Response response;
    bool read_ok = agrpc::read(*reader_writer, response, yield);

    grpc::Status status;
    bool finish_ok = agrpc::finish(*reader_writer, status, yield);
    // end-snippet

    silence_unused(request_ok, read_metadata_ok, write_ok, writes_done_ok, read_ok, finish_ok);
}

int main()
{
    auto stub =
        example::v1::Example::NewStub(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    // begin-snippet: create-grpc_context-client-side
    agrpc::GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    // end-snippet

    // begin-snippet: make-work-guard
    auto guard = boost::asio::make_work_guard(grpc_context);
    // end-snippet
    boost::asio::spawn(grpc_context,
                       [&](boost::asio::yield_context yield)
                       {
                           unary(grpc_context, *stub, yield);
                       });

    grpc_context.run();
}