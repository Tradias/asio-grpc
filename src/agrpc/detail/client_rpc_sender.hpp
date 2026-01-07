// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_DETAIL_CLIENT_RPC_SENDER_HPP
#define AGRPC_DETAIL_CLIENT_RPC_SENDER_HPP

#include <agrpc/detail/client_rpc_context_base.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/generic/generic_stub.h>

#include <agrpc/detail/asio_macros.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using ClientRPCAccess = detail::ClientRPCContextBaseAccess;

struct ClientContextCancellationFunction
{
    explicit ClientContextCancellationFunction(grpc::ClientContext& client_context) noexcept
        : client_context_(client_context)
    {
    }

    void operator()() const { client_context_.TryCancel(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type) const
    {
        if (static_cast<bool>(type & (asio::cancellation_type::terminal | asio::cancellation_type::partial)))
        {
            operator()();
        }
    }
#endif

    grpc::ClientContext& client_context_;
};

struct StatusSenderImplementationBase
{
    static constexpr bool NEEDS_ON_COMPLETE = true;

    using BaseType = detail::GrpcTagOperationBase;
    using Signature = void(grpc::Status);
    using StopFunction = detail::ClientContextCancellationFunction;

    grpc::Status status_{};
};

// Request unary
template <class Responder>
struct ClientUnaryRequestSenderImplementationBase;

template <class Response, template <class> class Responder>
struct ClientUnaryRequestSenderImplementationBase<Responder<Response>> : StatusSenderImplementationBase
{
    explicit ClientUnaryRequestSenderImplementationBase(std::unique_ptr<Responder<Response>> responder)
        : responder_(std::move(responder))
    {
    }

    template <class OnComplete>
    void complete(OnComplete on_complete, bool)
    {
        on_complete(static_cast<grpc::Status&&>(status_));
    }

    std::unique_ptr<Responder<Response>> responder_;
};

template <class Response>
struct ClientUnaryRequestSenderInitiation
{
    auto& stop_function_arg() const noexcept { return client_context_; }

    template <template <class> class Responder>
    void initiate(const agrpc::GrpcContext&, ClientUnaryRequestSenderImplementationBase<Responder<Response>>& impl,
                  void* tag) const
    {
        impl.responder_->StartCall();
        impl.responder_->Finish(&response_, &impl.status_, tag);
    }

    grpc::ClientContext& client_context_;
    Response& response_;
};

template <auto PrepareAsync>
struct ClientUnaryRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync>
struct ClientUnaryRequestSenderImplementation<PrepareAsync>
    : ClientUnaryRequestSenderImplementationBase<Responder<Response>>
{
    ClientUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                           grpc::ClientContext& client_context, const Request& req)
        : ClientUnaryRequestSenderImplementationBase<Responder<Response>>{
              (stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue())}
    {
    }
};

struct ClientGenericUnaryRequestSenderImplementation
    : ClientUnaryRequestSenderImplementationBase<grpc::GenericClientAsyncResponseReader>
{
    ClientGenericUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, const std::string& method,
                                                  grpc::GenericStub& stub, grpc::ClientContext& client_context,
                                                  const grpc::ByteBuffer& req)
        : ClientUnaryRequestSenderImplementationBase<grpc::GenericClientAsyncResponseReader>{
              stub.PrepareUnaryCall(&client_context, method, req, grpc_context.get_completion_queue())}
    {
    }
};

// Request streaming
template <class Derived>
struct ClientStreamingRequestSenderInitiationBase
{
    auto& stop_function_arg() const noexcept { return static_cast<const Derived&>(*this).rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ClientRPCAccess::responder(static_cast<const Derived&>(*this).rpc_).StartCall(tag);
    }
};

template <auto PrepareAsync, class Executor>
struct ClientStreamingRequestSenderInitiation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
struct ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>
    : ClientStreamingRequestSenderInitiationBase<ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>>
{
    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    ClientStreamingRequestSenderInitiation(RPC& rpc, Stub& stub, Response& response) : rpc_(rpc)
    {
        ClientRPCAccess::set_responder(
            rpc, (stub.*PrepareAsync)(&rpc.context(), &response,
                                      RPCExecutorBaseAccess::grpc_context(rpc).get_completion_queue()));
    }

    RPC& rpc_;
};

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
struct ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>
    : ClientStreamingRequestSenderInitiationBase<ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>>
{
    using RPC = detail::ClientRPCServerStreamingBase<PrepareAsync, Executor>;

    ClientStreamingRequestSenderInitiation(RPC& rpc, Stub& stub, const Request& req) : rpc_(rpc)
    {
        ClientRPCAccess::set_responder(
            rpc,
            (stub.*PrepareAsync)(&rpc.context(), req, RPCExecutorBaseAccess::grpc_context(rpc).get_completion_queue()));
    }

    RPC& rpc_;
};

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
struct ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>
    : ClientStreamingRequestSenderInitiationBase<ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>>
{
    using RPC = agrpc::ClientRPC<PrepareAsync, Executor>;

    ClientStreamingRequestSenderInitiation(RPC& rpc, Stub& stub) : rpc_(rpc)
    {
        ClientRPCAccess::set_responder(
            rpc, (stub.*PrepareAsync)(&rpc.context(), RPCExecutorBaseAccess::grpc_context(rpc).get_completion_queue()));
    }

    RPC& rpc_;
};

template <class Executor>
struct ClientStreamingRequestSenderInitiation<agrpc::ClientRPCType::GENERIC_STREAMING, Executor>
    : ClientStreamingRequestSenderInitiationBase<
          ClientStreamingRequestSenderInitiation<agrpc::ClientRPCType::GENERIC_STREAMING, Executor>>
{
    using RPC = agrpc::ClientRPC<agrpc::ClientRPCType::GENERIC_STREAMING, Executor>;

    ClientStreamingRequestSenderInitiation(RPC& rpc, const std::string& method, grpc::GenericStub& stub) : rpc_(rpc)
    {
        ClientRPCAccess::set_responder(
            rpc,
            stub.PrepareCall(&rpc.context(), method, RPCExecutorBaseAccess::grpc_context(rpc).get_completion_queue()));
    }

    RPC& rpc_;
};

struct ClientRPCGrpcSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ClientContextCancellationFunction;
};

struct ClientRPCSenderInitiationBase
{
    template <class Impl>
    static auto& stop_function_arg(Impl& impl) noexcept
    {
        return impl.rpc_.context();
    }
};

using ClientStreamingRequestSenderImplementation = ClientRPCGrpcSenderImplementation;

// Read initial metadata readable stream
using ClientReadInitialMetadataReadableStreamSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct ClientReadInitialMetadataReadableStreamSenderInitiation
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ClientRPCAccess::responder(rpc_).ReadInitialMetadata(tag);
    }

    detail::ClientRPCContextBase<Responder>& rpc_;
};

// Read
using ClientReadSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct GetResponseFromReabableStream;

template <template <class> class Responder, class Response>
struct GetResponseFromReabableStream<Responder<Response>>
{
    using Type = Response;
};

template <template <class, class> class Responder, class Request, class Response>
struct GetResponseFromReabableStream<Responder<Request, Response>>
{
    using Type = Response;
};

template <class Responder>
struct ClientReadSenderInitiation
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ClientRPCAccess::responder(rpc_).Read(&response_, tag);
    }

    detail::ClientRPCContextBase<Responder>& rpc_;
    typename GetResponseFromReabableStream<Responder>::Type& response_;
};

// Write
template <class Responder>
struct ClientWriteSenderImplementation : ClientRPCGrpcSenderImplementation
{
    explicit ClientWriteSenderImplementation(detail::ClientRPCContextBase<Responder>& rpc) noexcept : rpc_(rpc) {}

    void complete(const agrpc::GrpcContext&, bool ok)
    {
        ClientRPCAccess::set_writes_done(rpc_, ClientRPCAccess::is_writes_done(rpc_) || !ok);
    }

    detail::ClientRPCContextBase<Responder>& rpc_;
};

template <class Request>
struct ClientWriteSenderInitiation : ClientRPCSenderInitiationBase
{
    ClientWriteSenderInitiation(const Request& request, grpc::WriteOptions options)
        : request_(request), options_(options)
    {
    }

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ClientWriteSenderImplementation<Responder>& impl, void* tag) const
    {
        ClientRPCAccess::set_writes_done(impl.rpc_, options_.is_last_message());
        ClientRPCAccess::responder(impl.rpc_).Write(request_, options_, tag);
    }

    const Request& request_;
    grpc::WriteOptions options_;
};

// Read initial metadata writable stream
template <class Responder>
using ClientReadInitialMetadataWritableStreamSenderImplementation = ClientWriteSenderImplementation<Responder>;

struct ClientReadInitialMetadataWritableStreamSenderInitiation : ClientRPCSenderInitiationBase
{
    template <class Responder>
    static void initiate(const agrpc::GrpcContext&,
                         ClientReadInitialMetadataWritableStreamSenderImplementation<Responder>& impl, void* tag)
    {
        ClientRPCAccess::responder(impl.rpc_).ReadInitialMetadata(tag);
    }
};

// Writes done
template <class Responder>
struct ClientWritesDoneSenderImplementation : ClientRPCGrpcSenderImplementation
{
    explicit ClientWritesDoneSenderImplementation(detail::ClientRPCContextBase<Responder>& rpc) noexcept : rpc_(rpc) {}

    void complete(const agrpc::GrpcContext&, bool) { ClientRPCAccess::set_writes_done(rpc_, true); }

    detail::ClientRPCContextBase<Responder>& rpc_;
};

struct ClientWritesDoneSenderInitiation : ClientRPCSenderInitiationBase
{
    template <class Responder>
    static void initiate(const agrpc::GrpcContext&, const ClientWritesDoneSenderImplementation<Responder>& impl,
                         void* tag)
    {
        ClientRPCAccess::responder(impl.rpc_).WritesDone(tag);
    }
};

// Finish writeable stream
template <class Responder>
struct ClientFinishWritableStreamSenderImplementation : StatusSenderImplementationBase
{
    explicit ClientFinishWritableStreamSenderImplementation(detail::ClientRPCContextBase<Responder>& rpc) : rpc_(rpc) {}

    template <template <int> class OnComplete>
    void complete(OnComplete<0> on_complete, bool)
    {
        on_complete.grpc_context().work_started();
        ClientRPCAccess::responder(rpc_).Finish(&status_, on_complete.template tag<1>());
    }

    template <template <int> class OnComplete>
    void complete(OnComplete<1> on_complete, bool)
    {
        ClientRPCAccess::set_finished(rpc_);
        on_complete(static_cast<grpc::Status&&>(status_));
    }

    detail::ClientRPCContextBase<Responder>& rpc_;
};

struct ClientFinishWritableStreamSenderInitiation : ClientRPCSenderInitiationBase
{
    template <class Init, class Responder>
    static void initiate(Init init, ClientFinishWritableStreamSenderImplementation<Responder>& impl)
    {
        if (ClientRPCAccess::is_writes_done(impl.rpc_))
        {
            ClientRPCAccess::responder(impl.rpc_).Finish(&impl.status_, init.template tag<1>());
        }
        else
        {
            ClientRPCAccess::responder(impl.rpc_).WritesDone(init.template tag<0>());
        }
    }
};

// Finish readable stream
template <class Responder>
struct ClientFinishReadableStreamSenderImplementation;

template <template <class> class Responder, class Response>
struct ClientFinishReadableStreamSenderImplementation<Responder<Response>> : StatusSenderImplementationBase
{
    explicit ClientFinishReadableStreamSenderImplementation(detail::ClientRPCContextBase<Responder<Response>>& rpc)
        : rpc_(rpc)
    {
    }

    template <class OnComplete>
    void complete(OnComplete on_complete, bool)
    {
        ClientRPCAccess::set_finished(rpc_);
        on_complete(static_cast<grpc::Status&&>(status_));
    }

    detail::ClientRPCContextBase<Responder<Response>>& rpc_;
};

template <class Response>
struct ClientFinishUnarySenderInitation : ClientRPCSenderInitiationBase
{
    explicit ClientFinishUnarySenderInitation(Response& response) noexcept : response_(response) {}

    template <class Responder>
    void initiate(const agrpc::GrpcContext&, ClientFinishReadableStreamSenderImplementation<Responder>& impl,
                  void* tag) const
    {
        ClientRPCAccess::responder(impl.rpc_).Finish(&response_, &impl.status_, tag);
    }

    Response& response_;
};

struct ClientFinishServerStreamingSenderInitation : ClientRPCSenderInitiationBase
{
    template <class Responder>
    static void initiate(const agrpc::GrpcContext&, ClientFinishReadableStreamSenderImplementation<Responder>& impl,
                         void* tag)
    {
        ClientRPCAccess::responder(impl.rpc_).Finish(&impl.status_, tag);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CLIENT_RPC_SENDER_HPP
