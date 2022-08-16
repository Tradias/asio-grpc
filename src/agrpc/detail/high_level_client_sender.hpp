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
#include <agrpc/detail/high_level_client.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/tagged_ptr.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpcpp/generic/generic_stub.h>

AGRPC_NAMESPACE_BEGIN()

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor, agrpc::RPCType = detail::RPC_TYPE<PrepareAsync>>
class BasicRPC;

namespace detail
{
struct BasicRPCAccess
{
    template <class BasicRPC>
    static void client_initiate_finish(BasicRPC& rpc, void* tag)
    {
        rpc.set_finished();
        rpc.grpc_context().work_started();
        rpc.responder().Finish(&rpc.status(), tag);
    }

    template <class BasicRPC, class OnDone>
    static void bidi_done(detail::TaggedPtr<BasicRPC>& rpc, OnDone on_done, bool ok)
    {
        if (rpc.template has_bit<0>())
        {
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        if (!rpc->set_finished())
        {
            rpc.template set_bit<0>();
            auto& rpc_ref = *rpc;
            rpc_ref.grpc_context().work_started();
            rpc_ref.responder().Finish(&rpc_ref.status(), on_done.self());
            return;
        }
        on_done(false);
    }
};

template <auto PrepareAsync, class Executor>
struct ClientUnaryRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::ClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync, class Executor>
struct ClientUnaryRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>;
    using Signature = void(RPC);

    struct Initiation
    {
        Response& response;
    };

    ClientUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                           grpc::ClientContext& client_context, const Request& req)
        : rpc(grpc_context.get_executor()),
          responder((stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue()))
    {
    }

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        responder->StartCall();
        responder->Finish(&init->response, &rpc.status(), init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(std::move(rpc));
    }

    RPC rpc;
    std::unique_ptr<Responder<Response>> responder;
};

template <class Executor>
struct GenericClientUnaryRequestSenderImplementation : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<detail::GenericRPCType::CLIENT_UNARY, Executor, agrpc::RPCType::CLIENT_UNARY>;
    using Signature = void(RPC);

    struct Initiation
    {
        grpc::ByteBuffer& response;
    };

    GenericClientUnaryRequestSenderImplementation(agrpc::GrpcContext& grpc_context, const std::string& method,
                                                  grpc::GenericStub& stub, grpc::ClientContext& client_context,
                                                  const grpc::ByteBuffer& req)
        : rpc(grpc_context.get_executor()),
          responder(stub.PrepareUnaryCall(&client_context, method, req, grpc_context.get_completion_queue()))
    {
    }

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        responder->StartCall();
        responder->Finish(&init->response, &rpc.status(), init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        on_done(std::move(rpc));
    }

    RPC rpc;
    std::unique_ptr<grpc::GenericClientAsyncResponseReader> responder;
};

template <auto PrepareAsync, class Executor>
struct ClientClientStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
struct ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>;
    using Signature = void(RPC);
    using Initiation = detail::Empty;

    ClientClientStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, Response& response)
        : rpc(grpc_context.get_executor(), client_context,
              (stub.*PrepareAsync)(&client_context, &response, grpc_context.get_completion_queue()))
    {
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder().StartCall(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (ok)
        {
            on_done(std::move(rpc));
        }
        else
        {
            detail::BasicRPCAccess::client_initiate_finish(rpc, on_done.self());
        }
    }

    RPC rpc;
};

template <auto PrepareAsync, class Executor>
struct ClientServerStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
struct ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor> : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;
    using Signature = void(RPC);
    using Initiation = detail::Empty;

    ClientServerStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                     grpc::ClientContext& client_context, const Request& req)
        : rpc(grpc_context.get_executor(), client_context,
              (stub.*PrepareAsync)(&client_context, req, grpc_context.get_completion_queue()))
    {
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder().StartCall(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (ok)
        {
            on_done(std::move(rpc));
        }
        else
        {
            detail::BasicRPCAccess::client_initiate_finish(rpc, on_done.self());
        }
    }

    RPC rpc;
};

template <auto PrepareAsync, class Executor>
struct ClientBidirectionalStreamingRequestSenderImplementation;

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
struct ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>
    : detail::GrpcSenderImplementationBase
{
    using RPC = agrpc::BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;
    using Signature = void(RPC);
    using Initiation = detail::Empty;

    ClientBidirectionalStreamingRequestSenderImplementation(agrpc::GrpcContext& grpc_context, Stub& stub,
                                                            grpc::ClientContext& client_context)
        : rpc(grpc_context.get_executor(), client_context,
              (stub.*PrepareAsync)(&client_context, grpc_context.get_completion_queue()))
    {
    }

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc.responder().StartCall(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (ok)
        {
            on_done(std::move(rpc));
        }
        else
        {
            detail::BasicRPCAccess::client_initiate_finish(rpc, on_done.self());
        }
    }

    RPC rpc;
};

template <class BasicRPCBase>
struct ReadInitialMetadataSenderImplementation : detail::GrpcSenderImplementationBase
{
    using Initiation = detail::Empty;

    ReadInitialMetadataSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc->responder().ReadInitialMetadata(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (rpc.template has_bit<0>())
        {
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        rpc.template set_bit<0>();
        detail::BasicRPCAccess::client_initiate_finish(*rpc, on_done.self());
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};

template <class Responder, class Executor>
struct ReadServerStreamingSenderImplementation;

template <template <class> class Responder, class Response, class Executor>
struct ReadServerStreamingSenderImplementation<Responder<Response>, Executor> : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCClientServerStreamingBase<Responder<Response>, Executor>;

    struct Initiation
    {
        Response& response;
    };

    ReadServerStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        rpc->responder().Read(&init->response, init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (rpc.template has_bit<0>())
        {
            on_done(false);
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        rpc.template set_bit<0>();
        detail::BasicRPCAccess::client_initiate_finish(*rpc, on_done.self());
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};

template <class Responder, class Executor>
struct WriteClientStreamingSenderImplementation;

template <class Request, template <class> class Responder, class Executor>
struct WriteClientStreamingSenderImplementation<Responder<Request>, Executor> : detail::GrpcSenderImplementationBase
{
    static constexpr auto FINISHED_BIT = 0u;
    static constexpr auto LAST_MESSAGE_BIT = 1u;

    using BasicRPCBase = detail::BasicRPCClientClientStreamingBase<Responder<Request>, Executor>;

    struct Initiation
    {
        const Request& req;
        grpc::WriteOptions options;
    };

    WriteClientStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        auto& [req, options] = init.initiation();
        const auto is_last_message = options.is_last_message();
        if (is_last_message)
        {
            rpc.template set_bit<LAST_MESSAGE_BIT>();
            rpc->set_writes_done();
        }
        rpc->responder().Write(req, options, init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        if (rpc.template has_bit<FINISHED_BIT>())
        {
            on_done(rpc.template has_bit<LAST_MESSAGE_BIT>());
            return;
        }
        if (rpc.template has_bit<LAST_MESSAGE_BIT>())
        {
            rpc.template set_bit<LAST_MESSAGE_BIT>(ok);
            this->initiate_finish(on_done.self());
            return;
        }
        if (ok)
        {
            on_done(true);
            return;
        }
        this->initiate_finish(on_done.self());
    }

    void initiate_finish(void* self)
    {
        rpc.template set_bit<FINISHED_BIT>();
        detail::BasicRPCAccess::client_initiate_finish(*rpc, self);
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};

template <class BasicRPCBase>
struct ClientFinishSenderImplementation : detail::GrpcSenderImplementationBase
{
    using Initiation = detail::Empty;

    ClientFinishSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    void initiate(const agrpc::GrpcContext&, void* self) noexcept
    {
        auto& rpc_ref = *rpc;
        if (rpc_ref.is_writes_done())
        {
            rpc.template set_bit<0>();
            rpc_ref.responder().Finish(&rpc_ref.status(), self);
            rpc_ref.set_finished();
        }
        else
        {
            rpc_ref.responder().WritesDone(self);
        }
    }

    template <class OnDone>
    void done(OnDone on_done, bool)
    {
        if (rpc.template has_bit<0>())
        {
            on_done(rpc->ok());
            return;
        }
        rpc.template set_bit<0>();
        detail::BasicRPCAccess::client_initiate_finish(*rpc, on_done.self());
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};

template <class Responder, class Executor>
struct ClientReadBidiStreamingSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientReadBidiStreamingSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;

    struct Initiation
    {
        Response& response;
    };

    ClientReadBidiStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        rpc->responder().Read(&init->response, init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        detail::BasicRPCAccess::bidi_done(rpc, on_done, ok);
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};

template <class Responder, class Executor>
struct ClientWriteBidiStreamingSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientWriteBidiStreamingSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;

    struct Initiation
    {
        const Request& req;
        grpc::WriteOptions options;
    };

    ClientWriteBidiStreamingSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    template <class Init>
    void initiate(const agrpc::GrpcContext&, Init init) noexcept
    {
        auto& [req, options] = init.initiation();
        if (options.is_last_message())
        {
            rpc->set_writes_done();
        }
        rpc->responder().Write(req, options, init.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        detail::BasicRPCAccess::bidi_done(rpc, on_done, ok);
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};

template <class Responder, class Executor>
struct ClientWritesDoneSenderImplementation;

template <template <class, class> class Responder, class Request, class Response, class Executor>
struct ClientWritesDoneSenderImplementation<Responder<Request, Response>, Executor>
    : detail::GrpcSenderImplementationBase
{
    using BasicRPCBase = detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;
    using Initiation = detail::Empty;

    ClientWritesDoneSenderImplementation(BasicRPCBase& rpc) : rpc(rpc) {}

    void initiate(const agrpc::GrpcContext&, void* self) noexcept { rpc->responder().WritesDone(self); }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        rpc->set_writes_done();
        detail::BasicRPCAccess::bidi_done(rpc, on_done, ok);
    }

    detail::TaggedPtr<BasicRPCBase> rpc;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_HIGH_LEVEL_CLIENT_SENDER_HPP
