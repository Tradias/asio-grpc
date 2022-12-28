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
 * @c agrpc::RPC<PrepareAsync,Executor,agrpc::RPCType::CLIENT_UNARY> <br>
 * @c agrpc::RPC<agrpc::CLIENT_GENERIC_UNARY_RPC,Executor,agrpc::RPCType::CLIENT_UNARY> <br>
 * @c agrpc::RPC<PrepareAsync,Executor,agrpc::RPCType::CLIENT_CLIENT_STREAMING> <br>
 * @c agrpc::RPC<PrepareAsync,Executor,agrpc::RPCType::CLIENT_SERVER_STREAMING> <br>
 * @c agrpc::RPC<PrepareAsync,Executor,agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING> <br>
 * @c agrpc::RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC,Executor,agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING> <br>
 *
 * @since 2.1.0
 */
template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor, agrpc::RPCType = detail::RPC_TYPE<PrepareAsync>>
class RPC;

namespace detail
{
struct RPCAccess
{
    template <class RPC>
    static void client_initiate_finish(RPC& rpc, void* tag)
    {
        rpc.grpc_context().work_started();
        rpc.responder().Finish(&rpc.status(), tag);
    }
};

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

struct RPCCancellationFunction
{
#if !defined(AGRPC_UNIFEX)
    explicit
#endif
        RPCCancellationFunction(detail::RPCClientContextBase& rpc) noexcept
        : rpc_(rpc)
    {
    }

    void operator()() const { rpc_.cancel(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type) const
    {
        if (static_cast<bool>(type & (asio::cancellation_type::terminal | asio::cancellation_type::partial)))
        {
            operator()();
        }
    }
#endif

    detail::RPCClientContextBase& rpc_;
};

template <class Responder>
struct ClientUnaryRequestSenderImplementationBase;

template <class Response, template <class> class Responder>
struct ClientUnaryRequestSenderImplementationBase<Responder<Response>>
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(grpc::Status);
    using StopFunction = ClientContextCancellationFunction;

    struct Initiation
    {
        grpc::ClientContext& client_context_;
        Response& response_;
    };

    auto& stop_function_arg(const Initiation& initiation) noexcept { return initiation.client_context_; }

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
struct ClientStreamingRequestSenderImplementationBase
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using RPC = agrpc::RPC<PrepareAsync, Executor>;
    using Signature = void(RPC);
    using Initiation = detail::Empty;
    using StopFunction = detail::RPCCancellationFunction;

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

    void initiate(const agrpc::GrpcContext&, const Initiation&, void* self) noexcept
    {
        rpc_.responder().StartCall(self);
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool ok)
    {
        if (ok)
        {
            on_done(static_cast<RPC&&>(rpc_));
        }
        else
        {
            detail::RPCAccess::client_initiate_finish(rpc_, on_done.template self<1>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        rpc_.set_finished();
        on_done(static_cast<RPC&&>(rpc_));
    }

    RPC rpc_;
};

template <auto PrepareAsync, class Executor>
struct ClientClientStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
struct ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>
    : ClientStreamingRequestSenderImplementationBase<PrepareAsync, Executor>
{
    ClientClientStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, Response& response)
        : ClientStreamingRequestSenderImplementationBase<PrepareAsync, Executor>{
              {grpc_context.get_executor(), client_context,
               (stub.*PrepareAsync)(&client_context, &response, grpc_context.get_completion_queue())}}
    {
    }
};

template <auto PrepareAsync, class Executor>
struct ClientServerStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
struct ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>
    : ClientStreamingRequestSenderImplementationBase<PrepareAsync, Executor>
{
    ClientServerStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, const Request& req)
        : ClientStreamingRequestSenderImplementationBase<PrepareAsync, Executor>{
              {grpc_context.get_executor(), client_context,
               (stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue())}}
    {
    }
};

template <auto PrepareAsync, class Executor>
struct ClientBidirectionalStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
struct ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>
    : ClientStreamingRequestSenderImplementationBase<PrepareAsync, Executor>
{
    ClientBidirectionalStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                            grpc::ClientContext& client_context)
        : ClientStreamingRequestSenderImplementationBase<PrepareAsync, Executor>{
              {grpc_context.get_executor(), client_context,
               (stub.*PrepareAsync)(&client_context, grpc_context.get_completion_queue())}}
    {
    }
};

template <class Executor>
struct ClientGenericBidirectionalStreamingRequestSenderImplementation
    : ClientStreamingRequestSenderImplementationBase<detail::GenericRPCType::CLIENT_STREAMING, Executor>
{
    ClientGenericBidirectionalStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context,
                                                                   const std::string& method, grpc::GenericStub& stub,
                                                                   grpc::ClientContext& client_context)
        : ClientStreamingRequestSenderImplementationBase<detail::GenericRPCType::CLIENT_STREAMING, Executor>{
              {grpc_context.get_executor(), client_context,
               stub.PrepareCall(&client_context, method, grpc_context.get_completion_queue())}}
    {
    }
};

template <class RPCBase>
struct ReadInitialMetadataSenderImplementation : detail::GrpcSenderImplementationBase
{
    using Initiation = detail::Empty;
    using StopFunction = detail::RPCCancellationFunction;

    ReadInitialMetadataSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

    void initiate(const agrpc::GrpcContext&, const Initiation&, void* self) noexcept
    {
        rpc_.responder().ReadInitialMetadata(self);
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool ok)
    {
        if (ok)
        {
            on_done(true);
        }
        else
        {
            detail::RPCAccess::client_initiate_finish(rpc_, on_done.template self<1>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        rpc_.set_finished();
        on_done(false);
    }

    RPCBase& rpc_;
};

template <class Responder, class Executor>
struct ReadServerStreamingSenderImplementation;

template <template <class> class Responder, class Response, class Executor>
struct ReadServerStreamingSenderImplementation<Responder<Response>, Executor> : detail::GrpcSenderImplementationBase
{
    using RPCBase = detail::RPCClientServerStreamingBase<Responder<Response>, Executor>;
    using StopFunction = detail::RPCCancellationFunction;

    struct Initiation
    {
        Response& response_;
    };

    ReadServerStreamingSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

    void initiate(const agrpc::GrpcContext&, const Initiation& initiation, detail::OperationBase* operation) noexcept
    {
        rpc_.responder().Read(&initiation.response_, operation);
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool ok)
    {
        if (ok)
        {
            on_done(true);
        }
        else
        {
            detail::RPCAccess::client_initiate_finish(rpc_, on_done.template self<1>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        rpc_.set_finished();
        on_done(false);
    }

    RPCBase& rpc_;
};

template <class Responder, class Executor>
struct WriteClientStreamingSenderImplementation;

template <class Request, template <class> class Responder, class Executor>
struct WriteClientStreamingSenderImplementation<Responder<Request>, Executor> : detail::GrpcSenderImplementationBase
{
    using RPCBase = detail::RPCClientClientStreamingBase<Responder<Request>, Executor>;
    using StopFunction = detail::RPCCancellationFunction;

    struct Initiation
    {
        const Request& request_;
        grpc::WriteOptions options_;
    };

    WriteClientStreamingSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

    template <class Init>
    void initiate(Init init, const Initiation& initiation) noexcept
    {
        auto& [req, options] = initiation;
        if (options.is_last_message())
        {
            rpc_.set_writes_done();
            rpc_.responder().Write(req, options, init.template self<1>());
        }
        else
        {
            rpc_.responder().Write(req, options, init.template self<0>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool ok)
    {
        if (ok)
        {
            on_done(true);
        }
        else
        {
            detail::RPCAccess::client_initiate_finish(rpc_, on_done.template self<2>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool)
    {
        detail::RPCAccess::client_initiate_finish(rpc_, on_done.template self<2>());
    }

    template <template <int> class OnDone>
    void done(OnDone<2> on_done, bool)
    {
        rpc_.set_finished();
        on_done(rpc_.ok());
    }

    RPCBase& rpc_;
};

template <class RPCBase>
struct ClientFinishSenderImplementation : detail::GrpcSenderImplementationBase
{
    using Initiation = detail::Empty;
    using StopFunction = detail::RPCCancellationFunction;

    ClientFinishSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

    template <class Init>
    void initiate(Init init, const Initiation&) noexcept
    {
        if (rpc_.is_writes_done())
        {
            rpc_.responder().Finish(&rpc_.status(), init.template self<1>());
        }
        else
        {
            rpc_.responder().WritesDone(init.template self<0>());
        }
    }

    template <template <int> class OnDone>
    void done(OnDone<0> on_done, bool)
    {
        detail::RPCAccess::client_initiate_finish(rpc_, on_done.template self<1>());
    }

    template <template <int> class OnDone>
    void done(OnDone<1> on_done, bool) const
    {
        rpc_.set_finished();
        on_done(rpc_.ok());
    }

    RPCBase& rpc_;
};

template <class Responder, class Executor>
struct ClientReadBidiStreamingSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientReadBidiStreamingSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using RPCBase = detail::RPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;
    using StopFunction = detail::RPCCancellationFunction;

    struct Initiation
    {
        Response& response_;
    };

    ClientReadBidiStreamingSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

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
    using RPCBase = detail::RPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;
    using StopFunction = detail::RPCCancellationFunction;

    struct Initiation
    {
        const Request& request_;
        grpc::WriteOptions options_;
    };

    ClientWriteBidiStreamingSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

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
    using RPCBase = detail::RPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;
    using Initiation = detail::Empty;
    using StopFunction = detail::RPCCancellationFunction;

    ClientWritesDoneSenderImplementation(RPCBase& rpc) : rpc_(rpc) {}

    detail::RPCClientContextBase& stop_function_arg(const Initiation&) noexcept { return rpc_; }

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
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
