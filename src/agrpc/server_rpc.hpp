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

#ifndef AGRPC_AGRPC_SERVER_RPC_HPP
#define AGRPC_AGRPC_SERVER_RPC_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/server_rpc_context_base.hpp>
#include <agrpc/detail/server_rpc_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

struct DefaultServerRPCTraits
{
    static constexpr bool NOTIFY_WHEN_DONE = true;
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerUnaryRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class ServerRPC<RequestRPC, TraitsT, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::ServerRPCBase<grpc::ServerAsyncResponseWriter<ResponseT>, TraitsT::NOTIFY_WHEN_DONE>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncResponseWriter<ResponseT>;

  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::UNARY;

    using Request = RequestT;
    using Response = ResponseT;
    using Traits = TraitsT;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestRPC, TraitsT, OtherExecutor>;
    };

    explicit ServerRPC(const Executor& executor) : detail::RPCExecutorBase<Executor>{executor} {}

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, RequestT& request,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(),
            detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service, request},
            detail::ServerRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto send_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::SendInitialMetadataSenderInitiation<Responder>{*this},
            detail::SendInitialMetadataSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const ResponseT& response, const grpc::Status& status,
                CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithMessageInitation<Response>{response, status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish_with_error(const grpc::Status& status,
                           CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithErrorSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerClientStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class ServerRPC<RequestRPC, TraitsT, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::ServerRPCBase<grpc::ServerAsyncReader<ResponseT, RequestT>, TraitsT::NOTIFY_WHEN_DONE>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncReader<ResponseT, RequestT>;

  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::CLIENT_STREAMING;

    using Request = RequestT;
    using Response = ResponseT;
    using Traits = TraitsT;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestRPC, TraitsT, OtherExecutor>;
    };

    explicit ServerRPC(const Executor& executor) : detail::RPCExecutorBase<Executor>{executor} {}

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service},
            detail::ServerRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto send_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::SendInitialMetadataSenderInitiation<Responder>{*this},
            detail::SendInitialMetadataSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(RequestT& request, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerReadSenderInitiation<Responder>{*this, request},
            detail::ServerReadSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const ResponseT& response, const grpc::Status& status,
                CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithMessageInitation<Response>{response, status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish_with_error(const grpc::Status& status,
                           CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithErrorSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }
};

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerServerStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class ServerRPC<RequestRPC, TraitsT, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::ServerRPCBase<grpc::ServerAsyncWriter<ResponseT>, TraitsT::NOTIFY_WHEN_DONE>
{
  private:
    static constexpr bool IS_NOTIFY_WHEN_DONE = TraitsT::NOTIFY_WHEN_DONE;

    using Responder = grpc::ServerAsyncWriter<ResponseT>;

  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::SERVER_STREAMING;

    using Request = RequestT;
    using Response = ResponseT;
    using Traits = TraitsT;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestRPC, TraitsT, OtherExecutor>;
    };

    explicit ServerRPC(const Executor& executor) : detail::RPCExecutorBase<Executor>{executor} {}

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(ServiceT& service, RequestT& request,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(),
            detail::ServerRequestSenderInitiation<RequestRPC, IS_NOTIFY_WHEN_DONE>{service, request},
            detail::ServerRequestSenderImplementation<Responder, IS_NOTIFY_WHEN_DONE>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto send_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::SendInitialMetadataSenderInitiation<Responder>{*this},
            detail::SendInitialMetadataSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteSenderInitiation<Responder>{*this, response, options},
            detail::ServerWriteSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return write(response, {}, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, grpc::WriteOptions options, const grpc::Status& status,
                          CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteAndFinishSenderInitation<Response>{response, status, options},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, const grpc::Status& status,
                          CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return write_and_finish(response, {}, status, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const grpc::Status& status, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }
};

namespace detail
{
template <class RequestT, class ResponseT, template <class, class> class ResponderT, class TraitsT, class Executor>
class ServerRPCBidiStreamingBase<ResponderT<ResponseT, RequestT>, TraitsT, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::ServerRPCBase<ResponderT<ResponseT, RequestT>, TraitsT::NOTIFY_WHEN_DONE>
{
  private:
    using Responder = ResponderT<ResponseT, RequestT>;

  public:
    using Request = RequestT;
    using Response = ResponseT;
    using Traits = TraitsT;

    explicit ServerRPCBidiStreamingBase(const Executor& executor) : detail::RPCExecutorBase<Executor>{executor} {}

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto send_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::SendInitialMetadataSenderInitiation<Responder>{*this},
            detail::SendInitialMetadataSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(RequestT& request, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerReadSenderInitiation<Responder>{*this, request},
            detail::ServerReadSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteSenderInitiation<Responder>{*this, response, options},
            detail::ServerWriteSenderImplementation{}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return write(response, {}, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, grpc::WriteOptions options, const grpc::Status& status,
                          CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteAndFinishSenderInitation<Response>{response, status, options},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, const grpc::Status& status,
                          CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return write_and_finish(response, {}, status, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const grpc::Status& status, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, token);
    }
};
}

template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerBidiStreamingRequest<ServiceT, RequestT, ResponseT> RequestRPC, class TraitsT, class Executor>
class ServerRPC<RequestRPC, TraitsT, Executor>
    : public detail::ServerRPCBidiStreamingBase<grpc::ServerAsyncReaderWriter<ResponseT, RequestT>, TraitsT, Executor>
{
  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::BIDIRECTIONAL_STREAMING;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestRPC, TraitsT, OtherExecutor>;
    };

    using detail::ServerRPCBidiStreamingBase<grpc::ServerAsyncReaderWriter<ResponseT, RequestT>, TraitsT,
                                             Executor>::ServerRPCBidiStreamingBase;
};

template <class TraitsT, class Executor>
class ServerRPC<agrpc::ServerRPCType::GENERIC, TraitsT, Executor>
    : public detail::ServerRPCBidiStreamingBase<grpc::GenericServerAsyncReaderWriter, TraitsT, Executor>
{
  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::GENERIC;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<agrpc::ServerRPCType::GENERIC, TraitsT, OtherExecutor>;
    };

    using detail::ServerRPCBidiStreamingBase<grpc::GenericServerAsyncReaderWriter, TraitsT,
                                             Executor>::ServerRPCBidiStreamingBase;
};

template <class Traits = agrpc::DefaultServerRPCTraits, class Executor = agrpc::GrpcExecutor>
using GenericServerRPC = agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC, Traits, Executor>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_RPC_HPP
