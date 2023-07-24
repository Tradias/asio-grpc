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

#ifndef AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
#define AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/rpc_client_context_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/generic/generic_stub.h>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) Primary RPC template
 *
 * This is the main entrypoint into the high-level client API.
 *
 * @see
 * @c agrpc::RPC<UnaryPrepareAsync,Executor> <br>
 * @c agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC,Executor> <br>
 * @c agrpc::RPC<ClientStreamingPrepareAsync,Executor> <br>
 * @c agrpc::RPC<ServerStreamingPrepareAsync,Executor> <br>
 * @c agrpc::RPC<BidiStreamingPrepareAsync,Executor> <br>
 * @c agrpc::RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC,Executor> <br>
 *
 * @since 2.6.0
 */
template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
class RPC;

namespace detail
{
template <auto PrepareAsync, class Executor>
class ClientRPCUnaryBase;

template <auto PrepareAsync, class Executor>
class ClientRPCServerStreamingBase;

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

    struct Initiation
    {
        grpc::ClientContext& client_context_;
        Response& response_;
    };

    grpc::ClientContext& stop_function_arg(const Initiation& initiation) noexcept { return initiation.client_context_; }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, detail::OperationBase* operation) noexcept
    {
        responder_->StartCall();
        responder_->Finish(&initiation.response_, &status_, operation);
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(static_cast<grpc::Status&&>(status_));
    }

    std::unique_ptr<Responder<Response>> responder_;
    grpc::Status status_{};
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

template <auto PrepareAsync, class Executor>
struct ClientStreamingRequestSenderInitiation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
struct ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>
{
    using RPC = agrpc::RPC<PrepareAsync, Executor>;

    RPC& rpc_;

    ClientStreamingRequestSenderInitiation(RPC& rpc, Stub& stub, Response& response) : rpc_(rpc)
    {
        rpc.set_responder((stub.*PrepareAsync)(&rpc.context(), &response, rpc.grpc_context().get_completion_queue()));
    }
};

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
struct ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>
{
    using RPC = detail::ClientRPCServerStreamingBase<PrepareAsync, Executor>;

    RPC& rpc_;

    ClientStreamingRequestSenderInitiation(RPC& rpc, Stub& stub, const Request& req) : rpc_(rpc)
    {
        rpc.set_responder((stub.*PrepareAsync)(&rpc.context(), req, rpc.grpc_context().get_completion_queue()));
    }
};

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
struct ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>
{
    using RPC = agrpc::RPC<PrepareAsync, Executor>;

    RPC& rpc_;

    ClientStreamingRequestSenderInitiation(RPC& rpc, Stub& stub) : rpc_(rpc)
    {
        rpc.set_responder((stub.*PrepareAsync)(&rpc.context(), rpc.grpc_context().get_completion_queue()));
    }
};

template <class Executor>
struct ClientStreamingRequestSenderInitiation<detail::GenericRPCType::CLIENT_STREAMING, Executor>
{
    using RPC = agrpc::RPC<detail::GenericRPCType::CLIENT_STREAMING, Executor>;

    RPC& rpc_;

    ClientStreamingRequestSenderInitiation(RPC& rpc, const std::string& method, grpc::GenericStub& stub) : rpc_(rpc)
    {
        rpc.set_responder(stub.PrepareCall(&rpc.context(), method, rpc.grpc_context().get_completion_queue()));
    }
};

template <auto PrepareAsync, class Executor>
struct ClientStreamingRequestSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ClientContextCancellationFunction;
    using Initiation = detail::ClientStreamingRequestSenderInitiation<PrepareAsync, Executor>;

    auto& stop_function_arg(const Initiation& initiation) noexcept { return initiation.rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, void* self) noexcept
    {
        initiation.rpc_.responder().StartCall(self);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }
};

template <class RPC>
struct ReadInitialMetadataSenderImplementation : detail::GrpcSenderImplementationBase
{
    using Initiation = detail::Empty;
    using StopFunction = detail::ClientContextCancellationFunction;

    ReadInitialMetadataSenderImplementation(RPC& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation&, void* self) noexcept
    {
        rpc_.responder().ReadInitialMetadata(self);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    RPC& rpc_;
};

template <class RPC>
struct ReadServerStreamingSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ClientContextCancellationFunction;

    struct Initiation
    {
        typename RPC::Response& response_;
    };

    ReadServerStreamingSenderImplementation(RPC& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, detail::OperationBase* operation) noexcept
    {
        rpc_.responder().Read(&initiation.response_, operation);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    RPC& rpc_;
};

template <class RPC>
struct WriteClientStreamingSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = detail::ClientContextCancellationFunction;

    struct Initiation
    {
        const typename RPC::Request& request_;
        grpc::WriteOptions options_;
    };

    WriteClientStreamingSenderImplementation(RPC& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, void* self) noexcept
    {
        auto& [req, options] = initiation;
        if (options.is_last_message())
        {
            rpc_.set_writes_done();
            rpc_.responder().Write(req, options, self);
        }
        else
        {
            rpc_.responder().Write(req, options, self);
        }
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    RPC& rpc_;
};

template <class Responder, class Executor>
struct ClientReadBidiStreamingSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientReadBidiStreamingSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using RPCBase = detail::ClientRPCBidiStreamingBase<Responder<Request, Response>, Executor>;
    using StopFunction = detail::ClientContextCancellationFunction;

    struct Initiation
    {
        Response& response_;
    };

    ClientReadBidiStreamingSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, detail::OperationBase* operation) noexcept
    {
        rpc_.responder().Read(&initiation.response_, operation);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    RPCBase& rpc_;
};

template <class Responder, class Executor>
struct ClientWriteBidiStreamingSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientWriteBidiStreamingSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using RPCBase = detail::ClientRPCBidiStreamingBase<Responder<Request, Response>, Executor>;
    using StopFunction = detail::ClientContextCancellationFunction;

    struct Initiation
    {
        const Request& request_;
        grpc::WriteOptions options_;
    };

    ClientWriteBidiStreamingSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, detail::OperationBase* operation) noexcept
    {
        auto& [req, options] = initiation;
        if (options.is_last_message())
        {
            rpc_.set_writes_done();
        }
        rpc_.responder().Write(req, options, operation);
    }

    template <class OnDone>
    static void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }

    RPCBase& rpc_;
};

template <class Responder, class Executor>
struct ClientWritesDoneSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientWritesDoneSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using RPCBase = detail::ClientRPCBidiStreamingBase<Responder<Request, Response>, Executor>;
    using Initiation = detail::Empty;
    using StopFunction = detail::ClientContextCancellationFunction;

    ClientWritesDoneSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation&, void* self) noexcept
    {
        rpc_.responder().WritesDone(self);
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        rpc_.set_writes_done();
        on_done(ok);
    }

    RPCBase& rpc_;
};

template <class RPC>
struct ClientFinishSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(grpc::Status);
    using Initiation = detail::Empty;
    using StopFunction = detail::ClientContextCancellationFunction;

    ClientFinishSenderImplementation(RPC& rpc) : rpc_(rpc) {}

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    template <class Init>
    void initiate(Init init, const Initiation&) noexcept
    {
        if (rpc_.is_writes_done())
        {
            rpc_.responder().Finish(&status_, init.template self<1>());
        }
        else
        {
            rpc_.responder().WritesDone(init.template self<0>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool)
    {
        rpc_.grpc_context().work_started();
        rpc_.responder().Finish(&status_, on_done.template self<1>());
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        rpc_.set_finished();
        on_done(static_cast<grpc::Status&&>(status_));
    }

    RPC& rpc_;
    grpc::Status status_{};
};

template <class RPC>
struct ClientFinishServerStreamingSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(grpc::Status);
    using Initiation = detail::Empty;
    using StopFunction = detail::ClientContextCancellationFunction;

    grpc::ClientContext& stop_function_arg(const Initiation&) noexcept { return rpc_.context(); }

    void initiate(const agrpc::GrpcContext&, const Initiation&, void* self) noexcept
    {
        rpc_.responder().Finish(&status_, self);
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        rpc_.set_finished();
        on_done(static_cast<grpc::Status&&>(status_));
    }

    RPC& rpc_;
    grpc::Status status_{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
