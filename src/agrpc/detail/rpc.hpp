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

#ifndef AGRPC_DETAIL_RPC_HPP
#define AGRPC_DETAIL_RPC_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/grpc_context.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/server_context.h>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Message, class Responder>
struct ReadInitFunction
{
    Responder& responder_;
    Message& message_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Read(&message_, tag); }
};

template <class Message, class Responder>
struct WriteInitFunction
{
    Responder& responder_;
    const Message& message_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Write(message_, tag); }
};

template <class Message, class Responder>
struct WriteWithOptionsInitFunction
{
    Responder& responder_;
    const Message& message_;
    grpc::WriteOptions options_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Write(message_, options_, tag); }
};

template <class Message, class Responder>
struct WriteLastInitFunction
{
    Responder& responder_;
    const Message& message_;
    grpc::WriteOptions options_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.WriteLast(message_, options_, tag); }
};

template <class Responder>
struct ClientWritesDoneInitFunction
{
    Responder& responder_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.WritesDone(tag); }
};

template <class Responder>
struct ReadInitialMetadataInitFunction
{
    Responder& responder_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.ReadInitialMetadata(tag); }
};

template <class Responder, class = decltype(&Responder::Finish)>
struct FinishInitFunction;

template <class Responder, class BaseResponder>
struct FinishInitFunction<Responder, void (BaseResponder::*)(const grpc::Status&, void*)>
{
    static constexpr bool IS_CONST = true;

    Responder& responder_;
    const grpc::Status& status_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Finish(status_, tag); }
};

template <class Responder, class BaseResponder>
struct FinishInitFunction<Responder, void (BaseResponder::*)(grpc::Status*, void*)>
{
    static constexpr bool IS_CONST = false;

    Responder& responder_;
    grpc::Status& status_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Finish(&status_, tag); }
};

template <class Responder, class = decltype(&Responder::Finish)>
struct FinishWithMessageInitFunction;

template <class Responder, class BaseResponder, class Response>
struct FinishWithMessageInitFunction<Responder, void (BaseResponder::*)(const Response&, const grpc::Status&, void*)>
{
    static constexpr bool IS_CONST = true;

    using Message = Response;

    Responder& responder_;
    const Message& message_;
    const grpc::Status& status_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Finish(message_, status_, tag); }
};

template <class Responder, class BaseResponder, class Response>
struct FinishWithMessageInitFunction<Responder, void (BaseResponder::*)(Response*, grpc::Status*, void*)>
{
    static constexpr bool IS_CONST = false;

    using Message = Response;

    Responder& responder_;
    Message& message_;
    grpc::Status& status_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.Finish(&message_, &status_, tag); }
};

template <class Responder>
struct ServerFinishWithErrorInitFunction
{
    Responder& responder_;
    const grpc::Status& status_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.FinishWithError(status_, tag); }
};

template <class Message, class Responder>
struct ServerWriteAndFinishInitFunction
{
    Responder& responder_;
    const Message& message_;
    grpc::WriteOptions options_;
    const grpc::Status& status_;

    void operator()(const agrpc::GrpcContext&, void* tag) const
    {
        responder_.WriteAndFinish(message_, options_, status_, tag);
    }
};

template <class Responder>
struct SendInitialMetadataInitFunction
{
    Responder& responder_;

    void operator()(const agrpc::GrpcContext&, void* tag) const { responder_.SendInitialMetadata(tag); }
};

template <class Stub, class Request, class Responder>
struct AsyncClientServerStreamingRequestInitFunction
{
    detail::AsyncClientServerStreamingRequest<Stub, Request, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    const Request& request_;
    std::unique_ptr<Responder>& reader_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        reader_ = (stub_.*rpc_)(&client_context_, request_, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Request, class Responder>
struct PrepareAsyncClientServerStreamingRequestInitFunction
{
    detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    const Request& request_;
    std::unique_ptr<Responder>& reader_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        reader_ = (stub_.*rpc_)(&client_context_, request_, grpc_context.get_completion_queue());
        reader_->StartCall(tag);
    }
};

template <class Stub, class Request, class Responder>
struct AsyncClientServerStreamingRequestConvenienceInitFunction
{
    detail::AsyncClientServerStreamingRequest<Stub, Request, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    const Request& request_;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag) const
    {
        tag->completion_handler().payload() =
            (stub_.*rpc_)(&client_context_, request_, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Request, class Responder>
struct PrepareAsyncClientServerStreamingRequestConvenienceInitFunction
{
    detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    const Request& request_;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag) const
    {
        auto& reader = tag->completion_handler().payload();
        reader = (stub_.*rpc_)(&client_context_, request_, grpc_context.get_completion_queue());
        reader->StartCall(tag);
    }
};

template <class Stub, class Responder, class Response>
struct AsyncClientClientStreamingRequestInitFunction
{
    detail::AsyncClientClientStreamingRequest<Stub, Responder, Response> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    std::unique_ptr<Responder>& writer_;
    Response& response_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        writer_ = (stub_.*rpc_)(&client_context_, &response_, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder, class Response>
struct PrepareAsyncClientClientStreamingRequestInitFunction
{
    detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder, Response> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    std::unique_ptr<Responder>& writer_;
    Response& response_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        writer_ = (stub_.*rpc_)(&client_context_, &response_, grpc_context.get_completion_queue());
        writer_->StartCall(tag);
    }
};

template <class Stub, class Responder, class Response>
struct AsyncClientClientStreamingRequestConvenienceInitFunction
{
    detail::AsyncClientClientStreamingRequest<Stub, Responder, Response> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    Response& response_;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag) const
    {
        tag->completion_handler().payload() =
            (stub_.*rpc_)(&client_context_, &response_, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder, class Response>
struct PrepareAsyncClientClientStreamingRequestConvenienceInitFunction
{
    detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder, Response> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    Response& response_;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag) const
    {
        auto& writer = tag->completion_handler().payload();
        writer = (stub_.*rpc_)(&client_context_, &response_, grpc_context.get_completion_queue());
        writer->StartCall(tag);
    }
};

template <class Stub, class Responder>
struct AsyncClientBidirectionalStreamingRequestInitFunction
{
    detail::AsyncClientBidirectionalStreamingRequest<Stub, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    std::unique_ptr<Responder>& reader_writer_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        reader_writer_ = (stub_.*rpc_)(&client_context_, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder>
struct PrepareAsyncClientBidirectionalStreamingRequestInitFunction
{
    detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;
    std::unique_ptr<Responder>& reader_writer_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        reader_writer_ = (stub_.*rpc_)(&client_context_, grpc_context.get_completion_queue());
        reader_writer_->StartCall(tag);
    }
};

template <class Stub, class Responder>
struct AsyncClientBidirectionalStreamingRequestConvenienceInitFunction
{
    detail::AsyncClientBidirectionalStreamingRequest<Stub, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag) const
    {
        tag->completion_handler().payload() = (stub_.*rpc_)(&client_context_, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder>
struct PrepareAsyncClientBidirectionalStreamingRequestConvenienceInitFunction
{
    detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder> rpc_;
    Stub& stub_;
    grpc::ClientContext& client_context_;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag) const
    {
        auto& reader_writer = tag->completion_handler().payload();
        reader_writer = (stub_.*rpc_)(&client_context_, grpc_context.get_completion_queue());
        reader_writer->StartCall(tag);
    }
};

struct ClientGenericStreamingRequestInitFunction
{
    const std::string& method_;
    grpc::GenericStub& stub_;
    grpc::ClientContext& client_context_;
    std::unique_ptr<grpc::GenericClientAsyncReaderWriter>& reader_writer_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        reader_writer_ = stub_.PrepareCall(&client_context_, method_, grpc_context.get_completion_queue());
        reader_writer_->StartCall(tag);
    }
};

template <class Service, class Request, class Responder>
struct ServerMultiArgRequestInitFunction
{
    detail::ServerMultiArgRequest<Service, Request, Responder> rpc_;
    Service& service_;
    grpc::ServerContext& server_context_;
    Request& request_;
    Responder& responder_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        auto* const cq = grpc_context.get_server_completion_queue();
        (service_.*rpc_)(&server_context_, &request_, &responder_, cq, cq, tag);
    }
};

template <class Service, class Responder>
struct ServerSingleArgRequestInitFunction
{
    detail::ServerSingleArgRequest<Service, Responder> rpc_;
    Service& service_;
    grpc::ServerContext& server_context_;
    Responder& responder_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        auto* const cq = grpc_context.get_server_completion_queue();
        (service_.*rpc_)(&server_context_, &responder_, cq, cq, tag);
    }
};

template <class ReaderWriter>
struct ServerGenericRequestInitFunction
{
    grpc::AsyncGenericService& service_;
    grpc::GenericServerContext& server_context_;
    ReaderWriter& reader_writer_;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag) const
    {
        auto* const cq = grpc_context.get_server_completion_queue();
        service_.RequestCall(&server_context_, &reader_writer_, cq, cq, tag);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPC_HPP
