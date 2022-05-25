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

#ifndef AGRPC_DETAIL_RPC_HPP
#define AGRPC_DETAIL_RPC_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/grpcContext.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/server_context.h>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Stub, class Request, class Responder>
using ClientUnaryRequest = Responder (Stub::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*);

template <class Stub, class Request, class Responder>
using ClientServerStreamingRequest = Responder (Stub::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*,
                                                         void*);

template <class Stub, class Responder, class Response>
using ClientClientStreamingRequest = Responder (Stub::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*,
                                                         void*);

template <class Stub, class Responder>
using ClientBidirectionalStreamingRequest = Responder (Stub::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

template <class Service, class Request, class Responder>
using ServerMultiArgRequest = void (Service::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                                grpc::ServerCompletionQueue*, void*);

template <class Service, class Responder>
using ServerSingleArgRequest = void (Service::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                                 grpc::ServerCompletionQueue*, void*);

template <class Message, class Responder>
struct ReadInitFunction
{
    Responder& responder;
    Message& message;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Read(&message, tag); }
};

template <class Message, class Responder>
struct WriteInitFunction
{
    Responder& responder;
    const Message& message;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(message, tag); }
};

template <class Message, class Responder>
struct WriteWithOptionsInitFunction
{
    Responder& responder;
    const Message& message;
    grpc::WriteOptions options;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(message, options, tag); }
};

template <class Message, class Responder>
struct WriteLastInitFunction
{
    Responder& responder;
    const Message& message;
    grpc::WriteOptions options;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.WriteLast(message, options, tag); }
};

template <class Responder>
struct ClientWritesDoneInitFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.WritesDone(tag); }
};

template <class Responder>
struct ReadInitialMetadataInitFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.ReadInitialMetadata(tag); }
};

template <class Responder, class = decltype(&Responder::Finish)>
struct FinishInitFunction;

template <class Responder, class BaseResponder>
struct FinishInitFunction<Responder, void (BaseResponder::*)(const grpc::Status&, void*)>
{
    static constexpr bool IS_CONST = true;

    Responder& responder;
    const grpc::Status& status;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(status, tag); }
};

template <class Responder, class BaseResponder>
struct FinishInitFunction<Responder, void (BaseResponder::*)(grpc::Status*, void*)>
{
    static constexpr bool IS_CONST = false;

    Responder& responder;
    grpc::Status& status;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(&status, tag); }
};

template <class Responder, class = decltype(&Responder::Finish)>
struct FinishWithMessageInitFunction;

template <class Responder, class BaseResponder, class Response>
struct FinishWithMessageInitFunction<Responder, void (BaseResponder::*)(const Response&, const grpc::Status&, void*)>
{
    static constexpr bool IS_CONST = true;

    using Message = Response;

    Responder& responder;
    const Message& message;
    const grpc::Status& status;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(message, status, tag); }
};

template <class Responder, class BaseResponder, class Response>
struct FinishWithMessageInitFunction<Responder, void (BaseResponder::*)(Response*, grpc::Status*, void*)>
{
    static constexpr bool IS_CONST = false;

    using Message = Response;

    Responder& responder;
    Message& message;
    grpc::Status& status;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(&message, &status, tag); }
};

template <class Responder>
struct ServerFinishWithErrorInitFunction
{
    Responder& responder;
    const grpc::Status& status;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
};

template <class Message, class Responder>
struct ServerWriteAndFinishInitFunction
{
    Responder& responder;
    const Message& message;
    grpc::WriteOptions options;
    const grpc::Status& status;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.WriteAndFinish(message, options, status, tag); }
};

template <class Responder>
struct SendInitialMetadataInitFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.SendInitialMetadata(tag); }
};

template <class Stub, class Request, class Responder>
struct ClientServerStreamingRequestInitFunction
{
    detail::ClientServerStreamingRequest<Stub, Request, Responder> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    const Request& request;
    Responder& reader;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader = (stub.*rpc)(&client_context, request, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Request, class Responder>
struct ClientServerStreamingRequestConvenienceInitFunction
{
    detail::ClientServerStreamingRequest<Stub, Request, Responder> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    const Request& request;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag)
    {
        tag->completion_handler().payload() =
            (stub.*rpc)(&client_context, request, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder, class Response>
struct ClientClientStreamingRequestInitFunction
{
    detail::ClientClientStreamingRequest<Stub, Responder, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    Responder& writer;
    Response& response;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        writer = (stub.*rpc)(&client_context, &response, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder, class Response>
struct ClientClientStreamingRequestConvenienceInitFunction
{
    detail::ClientClientStreamingRequest<Stub, Responder, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    Response& response;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag)
    {
        tag->completion_handler().payload() =
            (stub.*rpc)(&client_context, &response, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder>
struct ClientBidirectionalStreamingRequestInitFunction
{
    detail::ClientBidirectionalStreamingRequest<Stub, Responder> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    Responder& reader_writer;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader_writer = (stub.*rpc)(&client_context, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Responder>
struct ClientBidirectionalStreamingRequestConvenienceInitFunction
{
    detail::ClientBidirectionalStreamingRequest<Stub, Responder> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag)
    {
        tag->completion_handler().payload() = (stub.*rpc)(&client_context, grpc_context.get_completion_queue(), tag);
    }
};

template <class ReaderWriter>
struct ClientGenericStreamingRequestInitFunction
{
    const std::string& method;
    grpc::GenericStub& stub;
    grpc::ClientContext& client_context;
    std::unique_ptr<ReaderWriter>& reader_writer;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader_writer = stub.PrepareCall(&client_context, method, grpc_context.get_completion_queue());
        reader_writer->StartCall(tag);
    }
};

template <class Service, class DerivedService, class Request, class Responder>
struct ServerMultiArgRequestInitFunction
{
    detail::ServerMultiArgRequest<Service, Request, Responder> rpc;
    DerivedService& service;
    grpc::ServerContext& server_context;
    Request& request;
    Responder& responder;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* const cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &request, &responder, cq, cq, tag);
    }
};

template <class Service, class DerivedService, class Responder>
struct ServerSingleArgRequestInitFunction
{
    detail::ServerSingleArgRequest<Service, Responder> rpc;
    DerivedService& service;
    grpc::ServerContext& server_context;
    Responder& responder;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* const cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &responder, cq, cq, tag);
    }
};

template <class ReaderWriter>
struct ServerGenericRequestInitFunction
{
    grpc::AsyncGenericService& service;
    grpc::GenericServerContext& server_context;
    ReaderWriter& reader_writer;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* const cq = grpc_context.get_server_completion_queue();
        service.RequestCall(&server_context, &reader_writer, cq, cq, tag);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPC_HPP
