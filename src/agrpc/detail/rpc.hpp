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

#include <grpcpp/alarm.h>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class RPC, class Request, class Reader>
using ClientUnaryRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*);

template <class RPC, class Request, class Reader>
using ClientServerStreamingRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*,
                                                     void*);

template <class RPC, class Writer, class Response>
using ClientClientStreamingRequest = Writer (RPC::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*, void*);

template <class RPC, class ReaderWriter>
using ClientBidirectionalStreamingRequest = ReaderWriter (RPC::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

template <class RPC, class Request, class Responder>
using ServerMultiArgRequest = void (RPC::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                            grpc::ServerCompletionQueue*, void*);

template <class RPC, class Responder>
using ServerSingleArgRequest = void (RPC::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                             grpc::ServerCompletionQueue*, void*);

template <class Deadline>
struct AlarmInitFunction
{
    grpc::Alarm& alarm;
    Deadline deadline;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        alarm.Set(grpc_context.get_completion_queue(), deadline, tag);
    }
};

template <class Deadline>
AlarmInitFunction(grpc::Alarm&, const Deadline&) -> AlarmInitFunction<Deadline>;

struct AlarmCancellationHandler
{
    grpc::Alarm& alarm;

    constexpr explicit AlarmCancellationHandler(grpc::Alarm& alarm) noexcept : alarm(alarm) {}

    template <class Deadline>
    constexpr explicit AlarmCancellationHandler(const detail::AlarmInitFunction<Deadline>& init_function) noexcept
        : alarm(init_function.alarm)
    {
    }

    void operator()() { alarm.Cancel(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type)
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            (*this)();
        }
    }
#endif
};

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

template <class RPC, class Stub, class Request, class Reader>
struct ClientServerStreamingRequestInitFunction
{
    detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    const Request& request;
    Reader& reader;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader = (stub.*rpc)(&client_context, request, grpc_context.get_completion_queue(), tag);
    }
};

template <class RPC, class Stub, class Request, class Reader>
ClientServerStreamingRequestInitFunction(detail::ClientServerStreamingRequest<RPC, Request, Reader>, Stub&,
                                         grpc::ClientContext&, const Request&, Reader&)
    -> ClientServerStreamingRequestInitFunction<RPC, Stub, Request, Reader>;

template <class RPC, class Stub, class Request, class Reader>
struct ClientServerStreamingRequestConvenienceInitFunction
{
    detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc;
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

template <class RPC, class Stub, class Request, class Reader>
ClientServerStreamingRequestConvenienceInitFunction(detail::ClientServerStreamingRequest<RPC, Request, Reader>, Stub&,
                                                    grpc::ClientContext&, const Request&)
    -> ClientServerStreamingRequestConvenienceInitFunction<RPC, Stub, Request, Reader>;

template <class RPC, class Stub, class Writer, class Response>
struct ClientClientStreamingRequestInitFunction
{
    detail::ClientClientStreamingRequest<RPC, Writer, Response> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    Writer& writer;
    Response& response;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        writer = (stub.*rpc)(&client_context, &response, grpc_context.get_completion_queue(), tag);
    }
};

template <class RPC, class Stub, class Writer, class Response>
ClientClientStreamingRequestInitFunction(detail::ClientClientStreamingRequest<RPC, Writer, Response>, Stub&,
                                         grpc::ClientContext&, Writer&, Response&)
    -> ClientClientStreamingRequestInitFunction<RPC, Stub, Writer, Response>;

template <class RPC, class Stub, class Writer, class Response>
struct ClientClientStreamingRequestConvenienceInitFunction
{
    detail::ClientClientStreamingRequest<RPC, Writer, Response> rpc;
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

template <class RPC, class Stub, class Writer, class Response>
ClientClientStreamingRequestConvenienceInitFunction(detail::ClientClientStreamingRequest<RPC, Writer, Response>, Stub&,
                                                    grpc::ClientContext&, Response&)
    -> ClientClientStreamingRequestConvenienceInitFunction<RPC, Stub, Writer, Response>;

template <class RPC, class Stub, class ReaderWriter>
struct ClientBidirectionalStreamingRequestInitFunction
{
    detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;
    ReaderWriter& reader_writer;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        reader_writer = (stub.*rpc)(&client_context, grpc_context.get_completion_queue(), tag);
    }
};

template <class RPC, class Stub, class ReaderWriter>
ClientBidirectionalStreamingRequestInitFunction(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter>, Stub&,
                                                grpc::ClientContext&, ReaderWriter&)
    -> ClientBidirectionalStreamingRequestInitFunction<RPC, Stub, ReaderWriter>;

template <class RPC, class Stub, class ReaderWriter>
struct ClientBidirectionalStreamingRequestConvenienceInitFunction
{
    detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc;
    Stub& stub;
    grpc::ClientContext& client_context;

    template <class T>
    void operator()(agrpc::GrpcContext& grpc_context, T* tag)
    {
        tag->completion_handler().payload() = (stub.*rpc)(&client_context, grpc_context.get_completion_queue(), tag);
    }
};

template <class RPC, class Stub, class ReaderWriter>
ClientBidirectionalStreamingRequestConvenienceInitFunction(
    detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter>, Stub&, grpc::ClientContext&)
    -> ClientBidirectionalStreamingRequestConvenienceInitFunction<RPC, Stub, ReaderWriter>;

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

template <class RPC, class Service, class Request, class Responder>
ServerMultiArgRequestInitFunction(detail::ServerMultiArgRequest<RPC, Request, Responder>, Service&,
                                  grpc::ServerContext&, Request&, Responder&)
    -> ServerMultiArgRequestInitFunction<RPC, Service, Request, Responder>;

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

template <class RPC, class Service, class Responder>
ServerSingleArgRequestInitFunction(detail::ServerSingleArgRequest<RPC, Responder>, Service&, grpc::ServerContext&,
                                   Responder&) -> ServerSingleArgRequestInitFunction<RPC, Service, Responder>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPCS_HPP
