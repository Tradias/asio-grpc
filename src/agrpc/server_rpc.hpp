// Copyright 2024 Dennis Hezel
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

#include <agrpc/default_server_rpc_traits.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/name.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/server_rpc_base.hpp>
#include <agrpc/detail/server_rpc_sender.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief I/O object for server-side, unary rpcs
 *
 * Use one of the `agrpc::register_` functions to set up request handling.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-unary
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam RequestUnary A pointer to the generated gRPC method.
 * @tparam Traits A type used to customize this rpc. See `agrpc::DefaultServerRPCTraits`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * (except `wait_for_done`) Terminal and partial. Cancellation is performed by invoking
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301).
 * After successful cancellation no further operations should be started on the rpc. Operations are also cancelled when
 * the deadline of the rpc has been reached.
 *
 * @since 2.7.0
 */
template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerUnaryRequest<ServiceT, RequestT, ResponseT> RequestUnary, class TraitsT, class Executor>
class ServerRPC<RequestUnary, TraitsT, Executor>
    : public detail::ServerRPCBase<grpc::ServerAsyncResponseWriter<ResponseT>, TraitsT, Executor>
{
  private:
    using Responder = grpc::ServerAsyncResponseWriter<ResponseT>;
    using Service = ServiceT;

  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::UNARY;

    /**
     * @brief The response message type
     */
    using Request = RequestT;

    /**
     * @brief The request message type
     */
    using Response = ResponseT;

    /**
     * @brief The traits type
     */
    using Traits = TraitsT;

    /**
     * @brief ServerRPCPtr specialized on this type
     */
    using Ptr = agrpc::ServerRPCPtr<ServerRPC>;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestUnary, TraitsT, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equal to the generated `Service::service_full_name()`.
     *
     * E.g. for the `.proto` schema
     *
     * @code{proto}
     * package example.v1;
     *
     * service Example { ... }
     * @endcode
     *
     * the return value would be `"example.v1.Example"`.
     */
    static constexpr std::string_view service_name() noexcept
    {
        return detail::SERVER_SERVICE_NAME_V<RequestUnary>.view();
    }

    /**
     * @brief Name of the gRPC method
     *
     * E.g. for `agrpc::ServerRPC<&example::Example::AsyncService::RequestMyMethod>` the return value would be
     * `"MyMethod"`.
     */
    static constexpr std::string_view method_name() noexcept
    {
        return detail::SERVER_METHOD_NAME_V<RequestUnary>.view();
    }

    /**
     * @brief Deleted default constructor
     */
    ServerRPC() = delete;

    ServerRPC(const ServerRPC& other) = delete;
    ServerRPC(ServerRPC&& other) = delete;
    ServerRPC& operator=(const ServerRPC& other) = delete;
    ServerRPC& operator=(ServerRPC&& other) = delete;

    /**
     * @brief Finish the rpc
     *
     * Indicate that the RPC is to be finished and request notification when the server has sent the appropriate
     * signals to the client to end the call. Should not be used concurrently with other operations.
     *
     * Side effect:
     *
     * @arg Also sends initial metadata if not already sent (using the ServerContext associated with the call).
     *
     * @note If status has a non-OK code, then message will not be sent, and the client will receive only the status
     * with possible trailing metadata.
     *
     * GRPC does not take ownership or a reference to message and status, so it is safe to deallocate once finish
     * returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const ResponseT& response, const grpc::Status& status, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithMessageInitation<Response>{response, status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Finish the rpc with an error
     *
     * Indicate that the stream is to be finished with a non-OK status, and request notification for when the server has
     * finished sending the appropriate signals to the client to end the call.
     *
     * It should not be called concurrently with other streaming APIs on the same stream.
     *
     * Side effect:
     *
     * @arg Sends initial metadata if not already sent (using the ServerContext associated with this call).
     *
     * GRPC does not take ownership or a reference to status, so it is safe to deallocate once finish_with_error
     * returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @note Status must have a non-OK code.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish_with_error(const grpc::Status& status, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithErrorSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ServerRPCContextBaseAccess;

    using detail::ServerRPCBase<Responder, TraitsT, Executor>::ServerRPCBase;
};

/**
 * @brief I/O object for server-side, client-streaming rpcs
 *
 * Use one of the `agrpc::register_` functions to set up request handling.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-client-streaming
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam RequestUnary A pointer to the generated gRPC method.
 * @tparam Traits A type used to customize this rpc. See `agrpc::DefaultServerRPCTraits`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * (except `wait_for_done`) Terminal and partial. Cancellation is performed by invoking
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301).
 * After successful cancellation no further operations should be started on the rpc. Operations are also cancelled when
 * the deadline of the rpc has been reached.
 *
 * @since 2.7.0
 */
template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerClientStreamingRequest<ServiceT, RequestT, ResponseT> RequestClientStreaming, class TraitsT,
          class Executor>
class ServerRPC<RequestClientStreaming, TraitsT, Executor>
    : public detail::ServerRPCBase<grpc::ServerAsyncReader<ResponseT, RequestT>, TraitsT, Executor>
{
  private:
    using Responder = grpc::ServerAsyncReader<ResponseT, RequestT>;
    using Service = ServiceT;

  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::CLIENT_STREAMING;

    /**
     * @brief The response message type
     */
    using Request = RequestT;

    /**
     * @brief The request message type
     */
    using Response = ResponseT;

    /**
     * @brief The traits type
     */
    using Traits = TraitsT;

    /**
     * @brief ServerRPCPtr specialized on this type
     */
    using Ptr = agrpc::ServerRPCPtr<ServerRPC>;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestClientStreaming, TraitsT, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equal to the generated `Service::service_full_name()`.
     *
     * E.g. for the `.proto` schema
     *
     * @code{proto}
     * package example.v1;
     *
     * service Example { ... }
     * @endcode
     *
     * the return value would be `"example.v1.Example"`.
     */
    static constexpr std::string_view service_name() noexcept
    {
        return detail::SERVER_SERVICE_NAME_V<RequestClientStreaming>.view();
    }

    /**
     * @brief Name of the gRPC method
     *
     * E.g. for `agrpc::ServerRPC<&example::Example::AsyncService::RequestMyMethod>` the return value would be
     * `"MyMethod"`.
     */
    static constexpr std::string_view method_name() noexcept
    {
        return detail::SERVER_METHOD_NAME_V<RequestClientStreaming>.view();
    }

    /**
     * @brief Deleted default constructor
     */
    ServerRPC() = delete;

    ServerRPC(const ServerRPC& other) = delete;
    ServerRPC(ServerRPC&& other) = delete;
    ServerRPC& operator=(const ServerRPC& other) = delete;
    ServerRPC& operator=(ServerRPC&& other) = delete;

    /**
     * @brief Receive a message from the client
     *
     * May not be called currently with `finish`/`finish_with_error`. It is not meaningful to call it concurrently with
     * another read on the same rpc since reads on the same stream are delivered in order.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that a valid message was read. `false` when
     * there will be no more incoming messages, either because the other side has called WritesDone() or the stream has
     * failed (or been cancelled).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(RequestT& req, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerReadSenderInitiation<Responder>{*this, req},
            detail::ServerReadSenderImplementation{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Finish the rpc
     *
     * Indicate that the stream is to be finished with a certain status code and also send out a response to the client.
     *
     * Should not be used concurrently with other operations and may only be called once.
     *
     * It is appropriate to call this method when:
     *
     * * all messages from the client have been received (either known implicitly, or explicitly because a previous read
     * operation completed with `false`).
     *
     * This operation will end when the server has finished sending out initial and trailing metadata, response message,
     * and status, or if some failure occurred when trying to do so.
     *
     * @note Response is not sent if status has a non-OK code.
     *
     * GRPC does not take ownership or a reference to `response` or `status`, so it is safe to deallocate once finish
     * returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const ResponseT& response, const grpc::Status& status, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithMessageInitation<Response>{response, status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Finish the rpc with an error
     *
     * Indicate that the stream is to be finished with a certain non-OK status code.
     *
     * Should not be used concurrently with other operations and may only be called once.
     *
     * This call is meant to end the call with some error, and can be called at any point that the server would like to
     * "fail" the call (except during `send_initial_metadata`).
     *
     * This operation will end when the server has finished sending out initial and trailing metadata and status, or if
     * some failure occurred when trying to do so.
     *
     * GRPC does not take ownership or a reference to `status`, so it is safe to to deallocate once finish_with_error
     * returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish_with_error(const grpc::Status& status, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishWithErrorSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ServerRPCContextBaseAccess;

    using detail::ServerRPCBase<Responder, TraitsT, Executor>::ServerRPCBase;
};

/**
 * @brief I/O object for server-side, server-streaming rpcs
 *
 * Use one of the `agrpc::register_` functions to set up request handling.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-server-streaming
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam RequestUnary A pointer to the generated gRPC method.
 * @tparam Traits A type used to customize this rpc. See `agrpc::DefaultServerRPCTraits`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * (except `wait_for_done`) Terminal and partial. Cancellation is performed by invoking
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301).
 * After successful cancellation no further operations should be started on the rpc. Operations are also cancelled when
 * the deadline of the rpc has been reached.
 *
 * @since 2.7.0
 */
template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerServerStreamingRequest<ServiceT, RequestT, ResponseT> RequestServerStreaming, class TraitsT,
          class Executor>
class ServerRPC<RequestServerStreaming, TraitsT, Executor>
    : public detail::ServerRPCBase<grpc::ServerAsyncWriter<ResponseT>, TraitsT, Executor>
{
  private:
    using Responder = grpc::ServerAsyncWriter<ResponseT>;
    using Service = ServiceT;

  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::SERVER_STREAMING;

    /**
     * @brief The response message type
     */
    using Request = RequestT;

    /**
     * @brief The request message type
     */
    using Response = ResponseT;

    /**
     * @brief The traits type
     */
    using Traits = TraitsT;

    /**
     * @brief ServerRPCPtr specialized on this type
     */
    using Ptr = agrpc::ServerRPCPtr<ServerRPC>;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestServerStreaming, TraitsT, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equal to the generated `Service::service_full_name()`.
     *
     * E.g. for the `.proto` schema
     *
     * @code{proto}
     * package example.v1;
     *
     * service Example { ... }
     * @endcode
     *
     * the return value would be `"example.v1.Example"`.
     */
    static constexpr std::string_view service_name() noexcept
    {
        return detail::SERVER_SERVICE_NAME_V<RequestServerStreaming>.view();
    }

    /**
     * @brief Name of the gRPC method
     *
     * E.g. for `agrpc::ServerRPC<&example::Example::AsyncService::RequestMyMethod>` the return value would be
     * `"MyMethod"`.
     */
    static constexpr std::string_view method_name() noexcept
    {
        return detail::SERVER_METHOD_NAME_V<RequestServerStreaming>.view();
    }

    /**
     * @brief Deleted default constructor
     */
    ServerRPC() = delete;

    ServerRPC(const ServerRPC& other) = delete;
    ServerRPC(ServerRPC&& other) = delete;
    ServerRPC& operator=(const ServerRPC& other) = delete;
    ServerRPC& operator=(ServerRPC&& other) = delete;

    /**
     * @brief Send a message to the client
     *
     * Only one write may be outstanding at any given time.
     *
     * GRPC does not take ownership or a reference to `response`, so it is safe to to deallocate once write returns,
     * unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, grpc::WriteOptions options, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteSenderInitiation<Responder>{*this, response, options},
            detail::ServerWriteSenderImplementation{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Send a message to the client (default WriteOptions)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, CompletionToken&& token = CompletionToken{})
    {
        return write(response, {}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Coalesce write and finish of this rpc
     *
     * Write response and coalesce it with trailing metadata which contains status, using WriteOptions
     * options.
     *
     * write_and_finish is equivalent of performing write with `WriteOptions.set_last_message()` and finish in a single
     * step.
     *
     * GRPC does not take ownership or a reference to response and status, so it is safe to deallocate once
     * write_and_finish returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is
     * used.
     *
     * Implicit input parameter:
     *
     * @arg The ServerContext associated with the call is used for sending trailing (and initial) metadata to the
     * client.
     *
     * @note Status must have an OK code.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, grpc::WriteOptions options, const grpc::Status& status,
                          CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteAndFinishSenderInitation<Response>{response, status, options},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Coalesce write and finish of this rpc (default WriteOptions)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, const grpc::Status& status,
                          CompletionToken&& token = CompletionToken{})
    {
        return write_and_finish(response, {}, status,
                                static_cast<CompletionToken&&>(static_cast<CompletionToken&&>(token)));
    }

    /**
     * @brief Finish this rpc
     *
     * Indicate that the stream is to be finished with a certain status code.
     *
     * Should not be used concurrently with other operations and may only be called once.
     *
     * This operation will end when the server has finished sending out initial metadata (if not sent already) and
     * status, or if some failure occurred when trying to do so.
     *
     * GRPC does not take ownership or a reference to status, so it is safe to to deallocate once finish
     * returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const grpc::Status& status, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ServerRPCContextBaseAccess;

    using detail::ServerRPCBase<Responder, TraitsT, Executor>::ServerRPCBase;
};

namespace detail
{
/**
 * @brief ServerRPC bidirectional-streaming base
 *
 * @since 2.7.0
 */
template <class RequestT, class ResponseT, template <class, class> class ResponderT, class TraitsT, class Executor>
class ServerRPCBidiStreamingBase<ResponderT<ResponseT, RequestT>, TraitsT, Executor>
    : public detail::ServerRPCBase<ResponderT<ResponseT, RequestT>, TraitsT, Executor>
{
  private:
    using Responder = ResponderT<ResponseT, RequestT>;

  public:
    /**
     * @brief The response message type
     */
    using Request = RequestT;

    /**
     * @brief The request message type
     */
    using Response = ResponseT;

    /**
     * @brief The traits type
     */
    using Traits = TraitsT;

    /**
     * @brief Receive a message from the client
     *
     * May not be called currently with `finish`/`write_and_finish`. It is not meaningful to call it concurrently with
     * another read on the same rpc since reads on the same stream are delivered in order.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that a valid message was read. `false` when there will be no more incoming
     * messages, either because the other side has called WritesDone() or the stream has failed (or been cancelled).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(RequestT& req, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerReadSenderInitiation<Responder>{*this, req},
            detail::ServerReadSenderImplementation{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Send a message to the client
     *
     * Only one write may be outstanding at any given time. It may not be called concurrently with operations other than
     * `read`.
     *
     * GRPC does not take ownership or a reference to `response`, so it is safe to to deallocate once write returns,
     * unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, grpc::WriteOptions options, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteSenderInitiation<Responder>{*this, response, options},
            detail::ServerWriteSenderImplementation{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Send a message to the client (default WriteOptions)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const ResponseT& response, CompletionToken&& token = CompletionToken{})
    {
        return write(response, {}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Coalesce write and finish of this rpc
     *
     * Write response and coalesce it with trailing metadata which contains status, using WriteOptions
     * options. May not be used concurrently with other operations.
     *
     * write_and_finish is equivalent of performing write with `WriteOptions.set_last_message()` and finish in a single
     * step.
     *
     * GRPC does not take ownership or a reference to response and status, so it is safe to deallocate once
     * write_and_finish returns, unless a deferred completion token like `agrpc::use_sender` or `asio::deferred` is
     * used.
     *
     * Implicit input parameter:
     *
     * @arg The ServerContext associated with the call is used for sending trailing (and initial) metadata to the
     * client.
     *
     * @note Status must have an OK code.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, grpc::WriteOptions options, const grpc::Status& status,
                          CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerWriteAndFinishSenderInitation<Response>{response, status, options},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Coalesce write and finish of this rpc (default WriteOptions)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write_and_finish(const ResponseT& response, const grpc::Status& status,
                          CompletionToken&& token = CompletionToken{})
    {
        return write_and_finish(response, {}, status, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Finish this rpc
     *
     * Indicate that the stream is to be finished with a certain status code.
     *
     * Completes when the server has sent the appropriate signals to the client to end the call.
     *
     * Should not be used concurrently with other operations and may only be called once.
     *
     * It is appropriate to call this method when either:
     *
     * * all messages from the client have been received (either known implicitly, or explicitly because a previous read
     * operation completed with `false`).
     * * it is desired to end the call early with some non-OK status code.
     *
     * This operation will end when the server has finished sending out initial metadata (if not sent already) and
     * status, or if some failure occurred when trying to do so.
     *
     * GRPC does not take ownership or a reference to status, so it is safe to to deallocate once Finish returns, unless
     * a deferred completion token like `agrpc::use_sender` or `asio::deferred` is used.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(const grpc::Status& status, CompletionToken&& token = CompletionToken{})
    {
        return detail::async_initiate_sender_implementation(
            this->grpc_context(), detail::ServerFinishSenderInitation{status},
            detail::ServerFinishSenderImplementation<Responder>{*this}, static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ServerRPCContextBaseAccess;

    using detail::ServerRPCBase<Responder, TraitsT, Executor>::ServerRPCBase;
};
}

/**
 * @brief I/O object for server-side, bidirectional-streaming rpcs
 *
 * Use one of the `agrpc::register_` functions to set up request handling.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-bidirectional-streaming
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam RequestUnary A pointer to the generated gRPC method.
 * @tparam Traits A type used to customize this rpc. See `agrpc::DefaultServerRPCTraits`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * (except `wait_for_done`) Terminal and partial. Cancellation is performed by invoking
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301).
 * After successful cancellation no further operations should be started on the rpc. Operations are also cancelled when
 * the deadline of the rpc has been reached.
 *
 * @since 2.7.0
 */
template <class ServiceT, class RequestT, class ResponseT,
          detail::ServerBidiStreamingRequest<ServiceT, RequestT, ResponseT> RequestBidiStreaming, class TraitsT,
          class Executor>
class ServerRPC<RequestBidiStreaming, TraitsT, Executor>
    : public detail::ServerRPCBidiStreamingBase<grpc::ServerAsyncReaderWriter<ResponseT, RequestT>, TraitsT, Executor>
{
  public:
    /**
     * @brief The rpc type
     */
    static constexpr agrpc::ServerRPCType TYPE = agrpc::ServerRPCType::BIDIRECTIONAL_STREAMING;

    /**
     * @brief ServerRPCPtr specialized on this type
     */
    using Ptr = agrpc::ServerRPCPtr<ServerRPC>;

    /**
     * @brief Rebind the ServerRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The ServerRPC type when rebound to the specified executor
         */
        using other = ServerRPC<RequestBidiStreaming, TraitsT, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equal to the generated `Service::service_full_name()`.
     *
     * E.g. for the `.proto` schema
     *
     * @code{proto}
     * package example.v1;
     *
     * service Example { ... }
     * @endcode
     *
     * the return value would be `"example.v1.Example"`.
     */
    static constexpr std::string_view service_name() noexcept
    {
        return detail::SERVER_SERVICE_NAME_V<RequestBidiStreaming>.view();
    }

    /**
     * @brief Name of the gRPC method
     *
     * E.g. for `agrpc::ServerRPC<&example::Example::AsyncService::RequestMyMethod>` the return value would be
     * `"MyMethod"`.
     */
    static constexpr std::string_view method_name() noexcept
    {
        return detail::SERVER_METHOD_NAME_V<RequestBidiStreaming>.view();
    }

    /**
     * @brief Deleted default constructor
     */
    ServerRPC() = delete;
    ServerRPC(const ServerRPC& other) = delete;
    ServerRPC(ServerRPC&& other) = delete;
    ServerRPC& operator=(const ServerRPC& other) = delete;
    ServerRPC& operator=(ServerRPC&& other) = delete;

  private:
    friend detail::ServerRPCContextBaseAccess;

    using Service = ServiceT;

    using detail::ServerRPCBidiStreamingBase<grpc::ServerAsyncReaderWriter<ResponseT, RequestT>, TraitsT,
                                             Executor>::ServerRPCBidiStreamingBase;
};

/**
 * @brief I/O object for server-side, generic rpcs
 *
 * Use one of the `agrpc::register_` functions to set up request handling.
 *
 * Example:
 *
 * @snippet server_rpc.cpp server-rpc-generic
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam RequestUnary A pointer to the generated gRPC method.
 * @tparam Traits A type used to customize this rpc. See `agrpc::DefaultServerRPCTraits`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * (except `wait_for_done`) Terminal and partial. Cancellation is performed by invoking
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301).
 * After successful cancellation no further operations should be started on the rpc. Operations are also cancelled when
 * the deadline of the rpc has been reached.
 *
 * @since 2.7.0
 */
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
     * @brief ServerRPCPtr specialized on this type
     */
    using Ptr = agrpc::ServerRPCPtr<ServerRPC>;

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

    /**
     * @brief Deleted default constructor
     */
    ServerRPC() = delete;
    ServerRPC(const ServerRPC& other) = delete;
    ServerRPC(ServerRPC&& other) = delete;
    ServerRPC& operator=(const ServerRPC& other) = delete;
    ServerRPC& operator=(ServerRPC&& other) = delete;

  private:
    friend detail::ServerRPCContextBaseAccess;

    using Service = grpc::AsyncGenericService;

    using detail::ServerRPCBidiStreamingBase<grpc::GenericServerAsyncReaderWriter, TraitsT,
                                             Executor>::ServerRPCBidiStreamingBase;
};

/**
 * @brief I/O object for server-side, generic rpcs (type alias)
 *
 * @see agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC,TraitsT,Executor>
 *
 * @since 2.7.0
 */
using GenericServerRPC = agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC>;

AGRPC_NAMESPACE_END

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_SERVER_RPC_HPP
