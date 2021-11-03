// Copyright 2021 Dennis Hezel
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
#include "agrpc/detail/attributes.hpp"
#include "agrpc/initiate.hpp"

#include <grpcpp/alarm.h>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

#include <utility>

namespace agrpc
{
namespace detail
{
template <class RPC, class Request, class Responder>
using ServerMultiArgRequest = void (RPC::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                            grpc::ServerCompletionQueue*, void*);

template <class RPC, class Responder>
using ServerSingleArgRequest = void (RPC::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                             grpc::ServerCompletionQueue*, void*);

template <class RPC, class Request, class Reader>
using ClientUnaryRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*);

template <class RPC, class Request, class Reader>
using ClientServerStreamingRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*,
                                                     void*);

template <class RPC, class Writer, class Response>
using ClientSideStreamingRequest = Writer (RPC::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*, void*);

template <class RPC, class ReaderWriter>
using ClientBidirectionalStreamingRequest = ReaderWriter (RPC::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

template <class Deadline>
struct AlarmFunction
{
    grpc::Alarm& alarm;
    Deadline deadline;

    void operator()(agrpc::GrpcContext& grpc_context, void* tag)
    {
        alarm.Set(grpc_context.get_completion_queue(), deadline, tag);
    }
};

template <class Deadline>
AlarmFunction(grpc::Alarm&, const Deadline&) -> AlarmFunction<Deadline>;

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
struct AlarmCancellationHandler
{
    grpc::Alarm& alarm;

    constexpr explicit AlarmCancellationHandler(grpc::Alarm& alarm) noexcept : alarm(alarm) {}

    void operator()(asio::cancellation_type type)
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            alarm.Cancel();
        }
    }
};
#endif

template <class RPC, class Service, class Request, class Responder>
struct ServerMultiArgRequestFunction
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
ServerMultiArgRequestFunction(detail::ServerMultiArgRequest<RPC, Request, Responder>, Service&, grpc::ServerContext&,
                              Request&, Responder&) -> ServerMultiArgRequestFunction<RPC, Service, Request, Responder>;

template <class RPC, class Service, class Responder>
struct ServerSingleArgRequestFunction
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
ServerSingleArgRequestFunction(detail::ServerSingleArgRequest<RPC, Responder>, Service&, grpc::ServerContext&,
                               Responder&) -> ServerSingleArgRequestFunction<RPC, Service, Responder>;

template <class Response, class Request>
struct ServerAsyncReaderWriterFunctions
{
    using Responder = grpc::ServerAsyncReaderWriter<Response, Request>;

    struct Read
    {
        Responder& responder;
        Request& request;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Read(&request, tag); }
    };

    struct Write
    {
        Responder& responder;
        const Response& response;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(response, tag); }
    };

    struct WriteAndFinish
    {
        Responder& responder;
        const Response& response;
        grpc::WriteOptions options;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag)
        {
            responder.WriteAndFinish(response, options, status, tag);
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
struct ServerAsyncReaderFunctions
{
    using Responder = grpc::ServerAsyncReader<Response, Request>;

    struct Read
    {
        Responder& responder;
        Request& request;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Read(&request, tag); }
    };

    struct Finish
    {
        Responder& responder;
        const Response& response;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(response, status, tag); }
    };

    struct FinishWithError
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
    };
};

template <class Response>
struct ServerAsyncWriterFunctions
{
    using Responder = grpc::ServerAsyncWriter<Response>;

    struct Write
    {
        Responder& responder;
        const Response& response;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(response, tag); }
    };

    struct Finish
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(status, tag); }
    };

    struct WriteAndFinish
    {
        Responder& responder;
        const Response& response;
        grpc::WriteOptions options;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag)
        {
            responder.WriteAndFinish(response, options, status, tag);
        }
    };
};

template <class Response>
struct ServerAsyncResponseWriterFunctions
{
    using Responder = grpc::ServerAsyncResponseWriter<Response>;

    struct Write
    {
        Responder& responder;
        const Response& response;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(response, status, tag); }
    };

    struct FinishWithError
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
    };
};

template <class Responder>
struct SendInitialMetadataFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.SendInitialMetadata(tag); }
};

template <class RPC, class Stub, class Request, class Reader>
struct ClientServerStreamingRequestFunction
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
ClientServerStreamingRequestFunction(detail::ClientServerStreamingRequest<RPC, Request, Reader>, Stub&,
                                     grpc::ClientContext&, const Request&, Reader&)
    -> ClientServerStreamingRequestFunction<RPC, Stub, Request, Reader>;

template <class RPC, class Stub, class Writer, class Response>
struct ClientSideStreamingRequestFunction
{
    detail::ClientSideStreamingRequest<RPC, Writer, Response> rpc;
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
ClientSideStreamingRequestFunction(detail::ClientSideStreamingRequest<RPC, Writer, Response>, Stub&,
                                   grpc::ClientContext&, Writer&, Response&)
    -> ClientSideStreamingRequestFunction<RPC, Stub, Writer, Response>;

template <class RPC, class Stub, class ReaderWriter>
struct ClientBidirectionalStreamingRequestFunction
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
ClientBidirectionalStreamingRequestFunction(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter>, Stub&,
                                            grpc::ClientContext&, ReaderWriter&)
    -> ClientBidirectionalStreamingRequestFunction<RPC, Stub, ReaderWriter>;

template <class Request, class Response>
struct ClientAsyncReaderWriterFunctions
{
    using Responder = grpc::ClientAsyncReaderWriter<Request, Response>;

    struct Read
    {
        Responder& responder;
        Response& response;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Read(&response, tag); }
    };

    struct Write
    {
        Responder& responder;
        const Request& request;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(request, tag); }
    };

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

template <class Request>
struct ClientAsyncWriterFunctions
{
    using Responder = grpc::ClientAsyncWriter<Request>;

    struct Write
    {
        Responder& responder;
        const Request& request;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(request, tag); }
    };

    struct WriteWithOptions
    {
        Responder& responder;
        const Request& request;
        grpc::WriteOptions options;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Write(request, options, tag); }
    };

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

template <class Response>
struct ClientAsyncReaderFunctions
{
    using Responder = grpc::ClientAsyncReader<Response>;

    struct Read
    {
        Responder& responder;
        Response& response;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Read(&response, tag); }
    };

    struct Finish
    {
        Responder& responder;
        grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(&status, tag); }
    };
};

template <class Response>
struct ClientAsyncResponseReaderFunctions
{
    using Responder = grpc::ClientAsyncResponseReader<Response>;

    struct Finish
    {
        Responder& responder;
        Response& response;
        grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.Finish(&response, &status, tag); }
    };

    struct FinishWithError
    {
        Responder& responder;
        const grpc::Status& status;

        void operator()(const agrpc::GrpcContext&, void* tag) { responder.FinishWithError(status, tag); }
    };
};

template <class Responder>
struct ReadInitialMetadataFunction
{
    Responder& responder;

    void operator()(const agrpc::GrpcContext&, void* tag) { responder.ReadInitialMetadata(tag); }
};
}  // namespace detail

template <class RPC, class Service, class Request, class Responder,
          class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
             grpc::ServerContext& server_context, Request& request, Responder& responder, CompletionToken token = {});

template <class RPC, class Service, class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, grpc::ServerContext& server_context,
             Responder& responder, CompletionToken token = {});

template <class RPCContextImplementationAllocator>
class RPCRequestContext;

namespace detail
{
struct RPCContextImplementation
{
    template <class RPCContextImplementationAllocator>
    static constexpr auto create(detail::AllocatedPointer<RPCContextImplementationAllocator>&& impl) noexcept
    {
        return agrpc::RPCRequestContext{std::move(impl)};
    }
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct RPCContextBase
{
    grpc::ServerContext context{};
};

template <class Request, class Responder>
struct MultiArgRPCContext : detail::RPCContextBase
{
    Responder responder{&this->context};
    Request request{};

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->request, this->responder,
                           std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->request, this->responder); }
};

template <class Responder>
struct SingleArgRPCContext : detail::RPCContextBase
{
    Responder responder{&this->context};

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->responder, std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->responder); }
};

template <class RPC, class Service, class RPCHandlerAllocator, class Handler>
struct RequestRepeater
{
    using executor_type = asio::associated_executor_t<Handler>;
    using allocator_type = asio::associated_allocator_t<Handler>;

    RPC rpc;
    Service& service;
    detail::AllocatedPointer<RPCHandlerAllocator> rpc_handler;
    Handler handler;

    RequestRepeater(RPC rpc, Service& service, detail::AllocatedPointer<RPCHandlerAllocator> rpc_handler,
                    Handler handler)
        : rpc(rpc), service(service), rpc_handler(std::move(rpc_handler)), handler(std::move(handler))
    {
    }

    void operator()(bool ok);

    executor_type get_executor() const noexcept { return asio::get_associated_executor(handler); }

    allocator_type get_allocator() const noexcept { return asio::get_associated_allocator(handler); }
};

template <class RPC, class Service, class Request, class Responder, class Handler>
void repeatedly_request(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service, Handler handler)
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(handler);
    auto rpc_handler = detail::allocate<detail::MultiArgRPCContext<Request, Responder>>(allocator);
    auto& context = rpc_handler->context;
    auto& request = rpc_handler->request;
    auto& responder = rpc_handler->responder;
    agrpc::request(rpc, service, context, request, responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_handler), std::move(handler)});
}

template <class RPC, class Service, class Responder, class Handler>
void repeatedly_request(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, Handler handler)
{
    const auto [executor, allocator] = detail::get_associated_executor_and_allocator(handler);
    auto rpc_handler = detail::allocate<detail::SingleArgRPCContext<Responder>>(allocator);
    auto& context = rpc_handler->context;
    auto& responder = rpc_handler->responder;
    agrpc::request(rpc, service, context, responder,
                   detail::RequestRepeater{rpc, service, std::move(rpc_handler), std::move(handler)});
}

template <class RPC, class Service, class RPCHandler, class Handler>
void RequestRepeater<RPC, Service, RPCHandler, Handler>::operator()(bool ok)
{
    if (ok) AGRPC_LIKELY
        {
            auto next_handler{this->handler};
            detail::repeatedly_request(this->rpc, this->service, std::move(next_handler));
        }
    std::move(this->handler)(detail::RPCContextImplementation::create(std::move(this->rpc_handler)), ok);
}
#endif
}  // namespace detail
}  // namespace agrpc

#endif  // AGRPC_DETAIL_RPCS_HPP
