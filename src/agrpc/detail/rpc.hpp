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

#ifndef AGRPC_DETAIL_RPCS_HPP
#define AGRPC_DETAIL_RPCS_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Stub, class Request, class Response>
using ClientUnaryRequest = std::unique_ptr<grpc::ClientAsyncResponseReader<Response>> (Stub::*)(grpc::ClientContext*,
                                                                                                const Request&,
                                                                                                grpc::CompletionQueue*);

template <class Stub, class Request, class Response>
using ClientServerStreamingRequest = std::unique_ptr<grpc::ClientAsyncReader<Response>> (Stub::*)(
    grpc::ClientContext*, const Request&, grpc::CompletionQueue*, void*);

template <class Stub, class Request, class Response>
using ClientClientStreamingRequest = std::unique_ptr<grpc::ClientAsyncWriter<Request>> (Stub::*)(grpc::ClientContext*,
                                                                                                 Response*,
                                                                                                 grpc::CompletionQueue*,
                                                                                                 void*);

template <class Stub, class Request, class Response>
using ClientBidirectionalStreamingRequest = std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Response>> (Stub::*)(
    grpc::ClientContext*, grpc::CompletionQueue*, void*);

template <class RPC, class Request, class Responder>
using ServerMultiArgRequest = void (RPC::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                            grpc::ServerCompletionQueue*, void*);

template <class RPC, class Responder>
using ServerSingleArgRequest = void (RPC::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                             grpc::ServerCompletionQueue*, void*);

template <class Message, class Responder>
struct BaseAsyncReaderInitFunctions
{
    struct Read
    {
        Responder& responder;
        Message& message;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Read(&message, tag); }
    };
};

template <class Message, class Responder>
struct BaseAsyncWriterInitFunctions
{
    struct Write
    {
        Responder& responder;
        const Message& message;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(message, tag); }
    };

    struct WriteWithOptions
    {
        Responder& responder;
        const Message& message;
        grpc::WriteOptions options;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(message, options, tag); }
    };

    struct WriteLast
    {
        Responder& responder;
        const Message& message;
        grpc::WriteOptions options;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.WriteLast(message, options, tag); }
    };
};

template <class Responder>
struct BaseClientAsyncStreamingInitFunctions
{
    struct WritesDone
    {
        Responder& responder;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.WritesDone(tag); }
    };

    struct Finish
    {
        Responder& responder;
        grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(&status, tag); }
    };
};

template <class Request, class Response>
struct ClientAsyncReaderWriterInitFunctions
    : detail::BaseClientAsyncStreamingInitFunctions<grpc::ClientAsyncReaderWriter<Request, Response>>,
      detail::BaseAsyncReaderInitFunctions<Response, grpc::ClientAsyncReaderWriter<Request, Response>>,
      detail::BaseAsyncWriterInitFunctions<Request, grpc::ClientAsyncReaderWriter<Request, Response>>
{
};

template <class Request>
struct ClientAsyncWriterInitFunctions : detail::BaseClientAsyncStreamingInitFunctions<grpc::ClientAsyncWriter<Request>>,
                                        detail::BaseAsyncWriterInitFunctions<Request, grpc::ClientAsyncWriter<Request>>
{
};

template <class Response>
struct ClientAsyncReaderInitFunctions
    : detail::BaseClientAsyncStreamingInitFunctions<grpc::ClientAsyncReader<Response>>,
      detail::BaseAsyncReaderInitFunctions<Response, grpc::ClientAsyncReader<Response>>
{
};

template <class Response>
struct ClientAsyncResponseReaderInitFunctions
{
    struct Finish
    {
        grpc::ClientAsyncResponseReader<Response>& responder;
        Response& response;
        grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(&response, &status, tag); }
    };
};

template <class Responder>
struct ReadInitialMetadataInitFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.ReadInitialMetadata(tag); }
};

template <class Message, class Responder>
struct BaseServerAsyncReaderInitFunctions
{
    struct Finish
    {
        Responder& responder;
        const Message& message;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(message, status, tag); }
    };

    struct FinishWithError
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
    };
};

template <class Message, class Responder>
struct BaseServerAsyncWriterInitFunctions
{
    struct WriteAndFinish
    {
        Responder& responder;
        const Message& message;
        grpc::WriteOptions options;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag)
        {
            responder.WriteAndFinish(message, options, status, tag);
        }
    };

    struct Finish
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(status, tag); }
    };
};

template <class Response, class Request>
struct ServerAsyncReaderWriterInitFunctions
    : detail::BaseAsyncReaderInitFunctions<Request, grpc::ServerAsyncReaderWriter<Response, Request>>,
      detail::BaseAsyncWriterInitFunctions<Response, grpc::ServerAsyncReaderWriter<Response, Request>>,
      detail::BaseServerAsyncWriterInitFunctions<Response, grpc::ServerAsyncReaderWriter<Response, Request>>
{
};

template <class Response, class Request>
struct ServerAsyncReaderInitFunctions
    : detail::BaseAsyncReaderInitFunctions<Request, grpc::ServerAsyncReader<Response, Request>>,
      detail::BaseServerAsyncReaderInitFunctions<Response, grpc::ServerAsyncReader<Response, Request>>
{
};

template <class Response>
struct ServerAsyncWriterInitFunctions
    : detail::BaseAsyncWriterInitFunctions<Response, grpc::ServerAsyncWriter<Response>>,
      detail::BaseServerAsyncWriterInitFunctions<Response, grpc::ServerAsyncWriter<Response>>
{
};

template <class Response>
struct ServerAsyncResponseWriterInitFunctions
    : detail::BaseServerAsyncReaderInitFunctions<Response, grpc::ServerAsyncResponseWriter<Response>>
{
};

template <class Responder>
struct SendInitialMetadataInitFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.SendInitialMetadata(tag); }
};

template <class Stub, class Request, class Response>
struct ClientServerStreamingRequestInitFunction
{
    detail::ClientServerStreamingRequest<Stub, Request, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    const Request& request;
    std::unique_ptr<grpc::ClientAsyncReader<Response>>& reader;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader = (stub.*rpc)(&client_context, request, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Request, class Response>
struct ClientServerStreamingRequestConvenienceInitFunction
{
    detail::ClientServerStreamingRequest<Stub, Request, Response> rpc;
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

template <class Stub, class Request, class Response>
struct ClientClientStreamingRequestInitFunction
{
    detail::ClientClientStreamingRequest<Stub, Request, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    std::unique_ptr<grpc::ClientAsyncWriter<Request>>& writer;
    Response& response;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        writer = (stub.*rpc)(&client_context, &response, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Request, class Response>
struct ClientClientStreamingRequestConvenienceInitFunction
{
    detail::ClientClientStreamingRequest<Stub, Request, Response> rpc;
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

template <class Stub, class Request, class Response>
struct ClientBidirectionalStreamingRequestInitFunction
{
    detail::ClientBidirectionalStreamingRequest<Stub, Request, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Response>>& reader_writer;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader_writer = (stub.*rpc)(&client_context, grpc_context.get_completion_queue(), tag);
    }
};

template <class Stub, class Request, class Response>
struct ClientBidirectionalStreamingRequestConvenienceInitFunction
{
    detail::ClientBidirectionalStreamingRequest<Stub, Request, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag)
    {
        tag->completion_handler().payload() = (stub.*rpc)(&client_context, grpc_context.get_completion_queue(), tag);
    }
};

template <class RPC, class Service, class Request, class Responder>
struct ServerMultiArgRequestInitFunction
{
    detail::ServerMultiArgRequest<RPC, Request, Responder> rpc;
    Service& service;
    grpc::ServerContext& server_context;
    Request& request;
    Responder& responder;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &request, &responder, cq, cq, tag);
    }
};

template <class RPC, class Service, class Responder>
struct ServerSingleArgRequestInitFunction
{
    detail::ServerSingleArgRequest<RPC, Responder> rpc;
    Service& service;
    grpc::ServerContext& server_context;
    Responder& responder;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        auto* cq = grpc_context.get_server_completion_queue();
        (service.*rpc)(&server_context, &responder, cq, cq, tag);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPCS_HPP
