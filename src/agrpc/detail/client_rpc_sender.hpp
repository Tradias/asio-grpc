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

#ifndef AGRPC_DETAIL_CLIENT_RPC_SENDER_HPP
#define AGRPC_DETAIL_CLIENT_RPC_SENDER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/rpc_client_context_base.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/generic/generic_stub.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <auto PrepareAsync, class Executor>
class ClientRPCUnaryBase;

using ClientRPCAccess = AutoCancelClientContextAndResponderAccess;

struct ClientContextCancellationFunction
{
#if !defined(AGRPC_UNIFEX)
    explicit
#endif
        ClientContextCancellationFunction(grpc::ClientContext& client_context) noexcept
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

template <class Responder>
struct ClientUnaryRequestSenderImplementationBase;

template <class Response, template <class> class Responder>
struct ClientUnaryRequestSenderImplementationBase<Responder<Response>>
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(grpc::Status);
    using StopFunction = detail::ClientContextCancellationFunction;

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(static_cast<grpc::Status&&>(status_));
    }

    std::unique_ptr<Responder<Response>> responder_;
    grpc::Status status_{};
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
          detail::ClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync>
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

struct ClientRPCGrpcSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ClientContextCancellationFunction;
};

using ClientStreamingRequestSenderImplementation = ClientRPCGrpcSenderImplementation;

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

using ReadInitialMetadataSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct ReadInitialMetadataSenderInitiation
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ClientRPCAccess::responder(rpc_).ReadInitialMetadata(tag);
    }

    detail::AutoCancelClientContextAndResponder<Responder>& rpc_;
};

using ReadServerStreamingSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct ReadServerStreamingSenderInitiation;

template <template <class> class Responder, class Response>
struct ReadServerStreamingSenderInitiation<Responder<Response>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ClientRPCAccess::responder(rpc_).Read(&response_, tag);
    }

    detail::AutoCancelClientContextAndResponder<Responder<Response>>& rpc_;
    Response& response_;
};

using WriteClientStreamingSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct WriteClientStreamingSenderInitiation;

template <template <class> class Responder, class Request>
struct WriteClientStreamingSenderInitiation<Responder<Request>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        if (options_.is_last_message())
        {
            ClientRPCAccess::set_writes_done(rpc_);
            ClientRPCAccess::responder(rpc_).Write(request_, options_, tag);
        }
        else
        {
            ClientRPCAccess::responder(rpc_).Write(request_, options_, tag);
        }
    }

    detail::AutoCancelClientContextAndResponder<Responder<Request>>& rpc_;
    const Request& request_;
    grpc::WriteOptions options_;
};

using ClientReadBidiStreamingSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct ClientReadBidiStreamingSenderInitiation;

template <template <class, class> class Responder, class Request, class Response>
struct ClientReadBidiStreamingSenderInitiation<Responder<Request, Response>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        ClientRPCAccess::responder(rpc_).Read(&response_, tag);
    }

    detail::AutoCancelClientContextAndResponder<Responder<Request, Response>>& rpc_;
    Response& response_;
};

using ClientWriteBidiStreamingSenderImplementation = ClientRPCGrpcSenderImplementation;

template <class Responder>
struct ClientWriteBidiStreamingSenderInitiation;

template <template <class, class> class Responder, class Request, class Response>
struct ClientWriteBidiStreamingSenderInitiation<Responder<Request, Response>>
{
    auto& stop_function_arg() const noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, void* tag) const
    {
        if (options_.is_last_message())
        {
            ClientRPCAccess::set_writes_done(rpc_);
        }
        ClientRPCAccess::responder(rpc_).Write(request_, options_, tag);
    }

    detail::AutoCancelClientContextAndResponder<Responder<Request, Response>>& rpc_;
    const Request& request_;
    grpc::WriteOptions options_;
};

template <class Responder>
struct ClientWritesDoneSenderImplementation : ClientRPCGrpcSenderImplementation
{
    explicit ClientWritesDoneSenderImplementation(detail::AutoCancelClientContextAndResponder<Responder>& rpc) noexcept
        : rpc_(rpc)
    {
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        ClientRPCAccess::set_writes_done(rpc_);
        on_done(ok);
    }

    detail::AutoCancelClientContextAndResponder<Responder>& rpc_;
};

struct ClientWritesDoneSenderInitiation
{
    template <class Responder>
    static auto& stop_function_arg(const ClientWritesDoneSenderImplementation<Responder>& impl) noexcept
    {
        return impl.rpc_.context();
    }

    template <class Responder>
    static void initiate(const agrpc::GrpcContext&, const ClientWritesDoneSenderImplementation<Responder>& impl,
                         void* tag)
    {
        ClientRPCAccess::responder(impl.rpc_).WritesDone(tag);
    }
};

template <class Responder>
struct ClientFinishSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(grpc::Status);
    using StopFunction = detail::ClientContextCancellationFunction;

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool)
    {
        on_done.grpc_context().work_started();
        ClientRPCAccess::responder(rpc_).Finish(&status_, on_done.template self<1>());
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        ClientRPCAccess::set_finished(rpc_);
        on_done(static_cast<grpc::Status&&>(status_));
    }

    detail::AutoCancelClientContextAndResponder<Responder>& rpc_;
    grpc::Status status_{};
};

struct ClientFinishSenderInitiation
{
    template <class RPC>
    static auto& stop_function_arg(const ClientFinishSenderImplementation<RPC>& impl) noexcept
    {
        return impl.rpc_.context();
    }

    template <class Init, class RPC>
    static void initiate(Init init, ClientFinishSenderImplementation<RPC>& impl)
    {
        if (ClientRPCAccess::is_writes_done(impl.rpc_))
        {
            ClientRPCAccess::responder(impl.rpc_).Finish(&impl.status_, init.template self<1>());
        }
        else
        {
            ClientRPCAccess::responder(impl.rpc_).WritesDone(init.template self<0>());
        }
    }
};

template <class Responder>
struct ClientFinishServerStreamingSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(grpc::Status);
    using Initiation = detail::Empty;
    using StopFunction = detail::ClientContextCancellationFunction;

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        ClientRPCAccess::set_finished(rpc_);
        on_done(static_cast<grpc::Status&&>(status_));
    }

    detail::AutoCancelClientContextAndResponder<Responder>& rpc_;
    grpc::Status status_{};
};

struct ClientFinishServerStreamingSenderInitation
{
    template <class Responder>
    static auto& stop_function_arg(const ClientFinishServerStreamingSenderImplementation<Responder>& impl) noexcept
    {
        return impl.rpc_.context();
    }

    template <class Responder>
    static void initiate(const agrpc::GrpcContext&, ClientFinishServerStreamingSenderImplementation<Responder>& impl,
                         void* tag)
    {
        ClientRPCAccess::responder(impl.rpc_).Finish(&impl.status_, tag);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CLIENT_RPC_SENDER_HPP
