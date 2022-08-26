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

#ifndef AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
#define AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/high_level_client.hpp>
#include <agrpc/detail/high_level_client_sender.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief (experimental) BasicRPC grpc::Status base
 *
 * @since 2.1.0
 */
class BasicRPCStatusBase
{
  public:
    /**
     * @brief The RPC's status code
     *
     * Equivalent to `status().error_code()`.
     */
    [[nodiscard]] grpc::StatusCode status_code() const noexcept { return status_.error_code(); }

    /**
     * @brief Is the RPC's status code OK?
     *
     * Equivalent to `status().ok()`.
     */
    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }

    /**
     * @brief The RPC's status
     */
    [[nodiscard]] grpc::Status& status() noexcept { return status_; };

    /**
     * @brief The RPC's status (const overload)
     */
    [[nodiscard]] const grpc::Status& status() const noexcept { return status_; };

  private:
    grpc::Status status_;
};

/**
 * @brief (experimental) BasicRPC executor base
 *
 * @since 2.1.0
 */
template <class Executor>
class BasicRPCExecutorBase
{
  public:
    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    /**
     * @brief Get the executor
     *
     * Thread-safe
     */
    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

  protected:
    BasicRPCExecutorBase() : executor(agrpc::GrpcExecutor{}) {}

    explicit BasicRPCExecutorBase(const Executor& executor) : executor(executor) {}

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor); }

  private:
    Executor executor;
};

/**
 * @brief (experimental) BasicRPC client-side client streaming base
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class RequestT, template <class> class ResponderT, class Executor>
class BasicRPCClientClientStreamingBase<ResponderT<RequestT>, Executor> : public detail::BasicRPCStatusBase,
                                                                          public detail::BasicRPCExecutorBase<Executor>,
                                                                          private detail::BasicRPCClientContextBase
{
  public:
    /**
     * @brief The request message type
     */
    using Request = RequestT;

    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * @attention If the server does not explicitly send initial metadata (e.g. by calling
     * `agrpc::send_initial_metadata`) but waits for a message from the client instead then this function won't
     * complete until `write()` is called.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead, the RPC is
     * automatically finished and error details can be obtained by calling `status()`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<BasicRPCClientClientStreamingBase>>(this->grpc_context(),
                                                                                                {}, {*this}, token);
    }

    /**
     * @brief Send a message to the server
     *
     * WriteOptions options is used to set the write options of this message, otherwise identical to:
     * `write(request, token)`. If WriteOptions contain `set_last_message` then the RPC is automatically finished as
     * part of this operation.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::WriteClientStreamingSenderImplementation<ResponderT<RequestT>, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    /**
     * @brief Send a message to the server
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to
     * `read_initial_metadata()`. gRPC does not take ownership or a reference to `request`, so it is safe to to
     * deallocate once write returns (unless a deferred completion token is used like `agrpc::use_sender` or
     * `asio::deferred`).
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc). The RPC is automatically finished in that case and error details can be obtained by calling
     * `status()`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Finish the RPC
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * Should not be used concurrently with other operations.
     *
     * This function may be called multiple times, but subsequent calls have no effect.
     *
     * The operation will finish when either:
     *
     * @arg The server has returned a status.
     * @arg The call failed for some reason and the library generated a status.
     *
     * Note that implementations of this method attempt to receive initial metadata from the server if initial metadata
     * has not been received yet.
     *
     * Side effect:
     *
     * @arg The ClientContext associated with the call is updated with possible initial and trailing metadata received
     * from the server.
     * @arg Attempts to fill in the response parameter that was passed to `BasicRPC::request`.
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool is equal to `ok()` after finishing.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientFinishSenderImplementation<BasicRPCClientClientStreamingBase>>(
            this->grpc_context(), {}, {*this}, !this->is_finished(), token, this->ok());
    }

    /**
     * @brief The underlying `grpc::ClientAsyncWriter/Interface`
     */
    [[nodiscard]] ResponderT<RequestT>& responder() noexcept { return *responder_; }

  protected:
    friend detail::ReadInitialMetadataSenderImplementation<BasicRPCClientClientStreamingBase>;
    friend detail::WriteClientStreamingSenderImplementation<ResponderT<RequestT>, Executor>;
    friend detail::ClientFinishSenderImplementation<BasicRPCClientClientStreamingBase>;
    friend detail::BasicRPCAccess;

    BasicRPCClientClientStreamingBase() = default;

    BasicRPCClientClientStreamingBase(const Executor& executor, grpc::ClientContext& client_context,
                                      std::unique_ptr<ResponderT<RequestT>>&& responder)
        : detail::BasicRPCExecutorBase<Executor>(executor),
          detail::BasicRPCClientContextBase(client_context),
          responder_(std::move(responder))
    {
    }

  private:
    std::unique_ptr<ResponderT<RequestT>> responder_;
};

/**
 * @brief (experimental) BasicRPC client-side server-streaming base
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class ResponseT, template <class> class ResponderT, class Executor>
class BasicRPCClientServerStreamingBase<ResponderT<ResponseT>, Executor>
    : public detail::BasicRPCStatusBase,
      public detail::BasicRPCExecutorBase<Executor>,
      private detail::BasicRPCClientContextBase
{
  public:
    /**
     * @brief The response message type
     */
    using Response = ResponseT;

    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the `read()` method.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead, the RPC is
     * automatically finished and error details can be obtained by calling `status()`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<BasicRPCClientServerStreamingBase>>(this->grpc_context(),
                                                                                                {}, {*this}, token);
    }

    /**
     * @brief Receive a message from the server
     *
     * Should not be called concurrently with `read_initial_metadata()`. It is not meaningful to call it concurrently
     * with another read on the same stream since reads on the same stream are delivered in order.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that a valid message was read. `false` when
     * there will be no more incoming messages, either because the other server is finished sending messages or the
     * stream has failed (or been cancelled). The RPC is automatically finished in either case and potential error
     * details can be obtained by calling `status()`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadServerStreamingSenderImplementation<ResponderT<ResponseT>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

    /**
     * @brief The underlying `grpc::ClientAsyncReader/Interface`
     */
    [[nodiscard]] ResponderT<ResponseT>& responder() noexcept { return *responder_; }

  protected:
    friend detail::ReadInitialMetadataSenderImplementation<BasicRPCClientServerStreamingBase>;
    friend detail::ReadServerStreamingSenderImplementation<ResponderT<ResponseT>, Executor>;
    friend detail::BasicRPCAccess;

    BasicRPCClientServerStreamingBase() = default;

    BasicRPCClientServerStreamingBase(const Executor& executor, grpc::ClientContext& client_context,
                                      std::unique_ptr<ResponderT<ResponseT>>&& responder)
        : detail::BasicRPCExecutorBase<Executor>(executor),
          detail::BasicRPCClientContextBase(client_context),
          responder_(std::move(responder))
    {
    }

  private:
    std::unique_ptr<ResponderT<ResponseT>> responder_;
};

/**
 * @brief (experimental) BasicRPC client-side bidirectional-streaming base
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class RequestT, class ResponseT, template <class, class> class ResponderT, class Executor>
class BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
    : public detail::BasicRPCStatusBase,
      public detail::BasicRPCExecutorBase<Executor>,
      private detail::BasicRPCClientContextBase
{
  public:
    /**
     * @brief The request message type
     */
    using Request = RequestT;

    /**
     * @brief The response message type
     */
    using Response = ResponseT;

    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the `read()` method.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead, the RPC is
     * automatically finished and error details can be obtained by calling `status()`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<BasicRPCBidirectionalStreamingBase>>(this->grpc_context(),
                                                                                                 {}, {*this}, token);
    }

    /**
     * @brief Receive a message from the server
     *
     * This is thread-safe with respect to `write()` or `writes_done()` methods. It should not be called concurrently
     * with other streaming APIs on the same stream. It is not meaningful to call it concurrently with another read on
     * the same stream since reads on the same stream are delivered in order.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that a valid message was read. `false` when
     * there will be no more incoming messages, either because the other server is finished sending messages or the
     * stream has failed (or been cancelled).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientReadBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

    /**
     * @brief Send a message to the server
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to
     * `read_initial_metadata()`. gRPC does not take ownership or a reference to `request`, so it is safe to to
     * deallocate once write returns (unless a deferred completion token is used like `agrpc::use_sender` or
     * `asio::deferred`).
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data is going to go to the wire. If it is `false`, it is
     * not going to the wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the
     * channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientWriteBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    /**
     * @brief Send a message to the server (default WriteOptions)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Signal WritesDone to the server
     *
     * This function may be called multiple times, but subsequent calls have no effect.
     *
     * Signal the client is done with the writes (half-close the client stream). Thread-safe with respect to read. May
     * not be called concurrently with a `write()` that has the
     * [last_message](https://grpc.github.io/grpc/cpp/classgrpc_1_1_write_options.html#ad930c28f5c32832e1d48ee30bf0858e3)
     * option set.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the data is going to go to the wire. If it is `false`, it is not going to the
     * wire because the call is already dead (i.e., canceled, deadline expired, other side dropped the channel, etc).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto writes_done(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientWritesDoneSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {}, {*this}, !this->is_writes_done() && !this->is_finished(), token, this->ok());
    }

    /**
     * @brief Signal WritesDone and finish the RPC
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * Should not be used concurrently with other operations.
     *
     * This function may be called multiple times, but subsequent calls have no effect.
     *
     * It is appropriate to call this method when:
     *
     * @arg All messages from the server have been received (either known implictly, or explicitly because a previous
     * read operation returned `false`).
     *
     * The operation will finish when either:
     *
     * @arg The server has returned a status.
     * @arg The call failed for some reason and the library generated a status.
     *
     * Note that implementations of this method attempt to receive initial metadata from the server if initial metadata
     * has not been received yet.
     *
     * Side effect:
     *
     * @arg The ClientContext associated with the call is updated with possible initial and trailing metadata received
     * from the server.
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool is equal to `ok()` after finishing.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientFinishSenderImplementation<BasicRPCBidirectionalStreamingBase>>(
            this->grpc_context(), {}, {*this}, !this->is_finished(), token, this->ok());
    }

    /**
     * @brief The underlying `grpc::ClientAsyncReaderWriter/Interface`
     */
    [[nodiscard]] ResponderT<RequestT, ResponseT>& responder() noexcept { return *responder_; }

  protected:
    friend detail::ReadInitialMetadataSenderImplementation<BasicRPCBidirectionalStreamingBase>;
    friend detail::ClientReadBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>;
    friend detail::ClientWriteBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>;
    friend detail::ClientWritesDoneSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>;
    friend detail::ClientFinishSenderImplementation<BasicRPCBidirectionalStreamingBase>;
    friend detail::BasicRPCAccess;

    BasicRPCBidirectionalStreamingBase() = default;

    BasicRPCBidirectionalStreamingBase(const Executor& executor, grpc::ClientContext& client_context,
                                       std::unique_ptr<ResponderT<RequestT, ResponseT>>&& responder)
        : detail::BasicRPCExecutorBase<Executor>(executor),
          detail::BasicRPCClientContextBase(client_context),
          responder_(std::move(responder))
    {
    }

  private:
    std::unique_ptr<ResponderT<RequestT, ResponseT>> responder_;
};
}

/**
 * @brief (experimental) A marker value to BasicRPC for generic unary RPCs
 *
 * @since 2.1.0
 */
inline constexpr auto CLIENT_GENERIC_UNARY_RPC = detail::GenericRPCType::CLIENT_UNARY;

/**
 * @brief (experimental) A marker value to BasicRPC for generic streaming RPCs
 *
 * @since 2.1.0
 */
inline constexpr auto CLIENT_GENERIC_STREAMING_RPC = detail::GenericRPCType::CLIENT_STREAMING;

/**
 * @brief (experimental) I/O object for client-side unary RPCs
 *
 * @tparam PrepareAsync A pointer to the async version of the RPC method. The async version starts with `PrepareAsync`.
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::ClientUnaryRequest<StubT, RequestT, ResponderT<ResponseT>> PrepareAsync, class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>
{
  public:
    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief The response message type
     */
    using Request = RequestT;

    /**
     * @brief The request message type
     */
    using Response = ResponseT;

    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    /**
     * @brief Rebind the BasicRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicRPC type when rebound to the specified executor
         */
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    /**
     * @brief Start a unary request
     *
     * @param response The response message, will be filled by the server upon finishing this RPC.
     * @param request The request message, save to delete when this function returns, unless a deferred completion token
     * is used like `agrpc::use_sender` or `asio::deferred`.
     * @param response The response message, will be filled by the server upon finishing this RPC. Must remain alive
     * until this RPC is finished.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(grpc::Status)`. Use `grpc::Status::ok()` to check whether the request was successful.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        const RequestT& request, ResponseT& response,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {response}, {grpc_context, stub, context, request}, token);
    }

    /**
     * @brief Start a generic unary request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, const RequestT& request,
                        ResponseT& response, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, response,
                                 static_cast<CompletionToken&&>(token));
    }
};

/**
 * @brief (experimental) I/O object for client-side generic unary RPCs
 *
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class Executor>
class BasicRPC<agrpc::CLIENT_GENERIC_UNARY_RPC, Executor, agrpc::RPCType::CLIENT_UNARY>
{
  public:
    /**
     * @brief The stub type
     */
    using Stub = grpc::GenericStub;

    /**
     * @brief The response message type
     */
    using Request = grpc::ByteBuffer;

    /**
     * @brief The request message type
     */
    using Response = grpc::ByteBuffer;

    /**
     * @brief The executor type
     */
    using executor_type = Executor;

    /**
     * @brief Rebind the BasicRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicRPC type when rebound to the specified executor
         */
        using other = BasicRPC<agrpc::CLIENT_GENERIC_UNARY_RPC, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    /**
     * @brief Start a generic unary request
     *
     * @param method The RPC method to call, e.g. "/test.v1.Test/Unary"
     * @param response The response message, will be filled by the server upon finishing this RPC.
     * @param request The request message, save to delete when this function returns, unless a deferred completion token
     * is used like `agrpc::use_sender` or `asio::deferred`.
     * @param response The response message, will be filled by the server upon finishing this RPC. Must remain alive
     * until this RPC is finished.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(grpc::Status)`. Use `grpc::Status::ok()` to check whether the request was successful.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context, const grpc::ByteBuffer& request, grpc::ByteBuffer& response,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::GenericClientUnaryRequestSenderImplementation<Executor>>(
            grpc_context, {response}, {grpc_context, method, stub, context, request}, token);
    }

    /**
     * @brief Start a generic unary request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context, const grpc::ByteBuffer& request, grpc::ByteBuffer& response,
                        CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), method, stub, context, request, response,
                                 static_cast<CompletionToken&&>(token));
    }
};

/**
 * @brief (experimental) I/O object for client-side client-streaming RPCs
 *
 * @tparam PrepareAsync A pointer to the async version of the RPC method. The async version starts with `PrepareAsync`.
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::PrepareAsyncClientClientStreamingRequest<StubT, ResponderT<RequestT>, ResponseT> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>
    : public detail::BasicRPCClientClientStreamingBase<ResponderT<RequestT>, Executor>
{
  public:
    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief The response message type
     */
    using Response = ResponseT;

    /**
     * @brief Rebind the BasicRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicRPC type when rebound to the specified executor
         */
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>;
    };

    /**
     * @brief Start a client-streaming request
     *
     * @attention This function may not be used with the
     * [initial_metadata_corked](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#af79c64534c7b208594ba8e76021e2696)
     * option set.
     *
     * @param stub The Stub that corresponds to the RPC method, e.g. `example::v1::Example::Stub`.
     * @param response The response message, will be filled by the server upon finishing this RPC. Must remain alive
     * until this RPC is finished.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(BasicRPC)`. Use `ok()` to check whether the request was successful.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, response}, token);
    }

    /**
     * @brief Start a client-streaming request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, ResponseT& response,
                        CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, response,
                                 static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCClientClientStreamingBase<ResponderT<RequestT>, Executor>::BasicRPCClientClientStreamingBase;
};

/**
 * @brief (experimental) I/O object for client-side server-streaming RPCs
 *
 * @tparam PrepareAsync A pointer to the async version of the RPC method. The async version starts with `PrepareAsync`.
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::PrepareAsyncClientServerStreamingRequest<StubT, RequestT, ResponderT<ResponseT>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>
    : public detail::BasicRPCClientServerStreamingBase<ResponderT<ResponseT>, Executor>
{
  public:
    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief The request message type
     */
    using Request = RequestT;

    /**
     * @brief Rebind the BasicRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicRPC type when rebound to the specified executor
         */
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;
    };

    /**
     * @brief Start a server-streaming request
     *
     * @param stub The Stub that corresponds to the RPC method, e.g. `example::v1::Example::Stub`.
     * @param request The request message, save to delete when this function returns, unless a deferred completion token
     * is used like `agrpc::use_sender` or `asio::deferred`.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(BasicRPC)`. Use `ok()` to check whether the request was successful.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        const RequestT& request, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, request}, token);
    }

    /**
     * @brief Start a server-streaming request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, const RequestT& request,
                        CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request,
                                 static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCClientServerStreamingBase<ResponderT<ResponseT>, Executor>::BasicRPCClientServerStreamingBase;
};

/**
 * @brief (experimental) I/O object for client-side bidirectional-streaming RPCs
 *
 * @tparam PrepareAsync A pointer to the async version of the RPC method. The async version starts with `PrepareAsync`.
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class StubT, class RequestT, class ResponseT, template <class, class> class ResponderT,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<StubT, ResponderT<RequestT, ResponseT>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>
    : public detail::BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
{
  public:
    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief Rebind the BasicRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicRPC type when rebound to the specified executor
         */
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;
    };

    /**
     * @brief Start a bidirectional-streaming request
     *
     * @param stub The Stub that corresponds to the RPC method, e.g. `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(BasicRPC)`. Use `ok()` to check whether the request was successful.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context}, token);
    }

    /**
     * @brief Start a bidirectional-streaming request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context,
                        CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context,
                                 static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>,
                                                     Executor>::BasicRPCBidirectionalStreamingBase;
};

/**
 * @brief (experimental) I/O object for client-side generic streaming RPCs
 *
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)).
 *
 * @since 2.1.0
 */
template <class Executor>
class BasicRPC<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>
    : public detail::BasicRPCBidirectionalStreamingBase<grpc::GenericClientAsyncReaderWriter, Executor>
{
  public:
    /**
     * @brief The stub type
     */
    using Stub = grpc::GenericStub;

    /**
     * @brief Rebind the BasicRPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicRPC type when rebound to the specified executor
         */
        using other = BasicRPC<agrpc::CLIENT_GENERIC_STREAMING_RPC, OtherExecutor,
                               agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;
    };

    /**
     * @brief Start a generic streaming request
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(BasicRPC)`. Use `ok()` to check whether the request was successful.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientBidirectionalStreamingRequestSenderImplementation<agrpc::CLIENT_GENERIC_STREAMING_RPC,
                                                                            Executor>>(
            grpc_context, {}, {grpc_context, method, stub, context}, token);
    }

    /**
     * @brief Start a generic streaming request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), method, stub, context,
                                 static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ClientBidirectionalStreamingRequestSenderImplementation<agrpc::CLIENT_GENERIC_STREAMING_RPC,
                                                                           Executor>;

    using detail::BasicRPCBidirectionalStreamingBase<grpc::GenericClientAsyncReaderWriter,
                                                     Executor>::BasicRPCBidirectionalStreamingBase;
};

/**
 * @brief (experimental) A BasicRPC that uses `agrpc::DefaultCompletionToken`
 *
 * This is the main entrypoint into the high-level client API. See BasicRPC for details.
 *
 * To use a different default completion token apply the `as_default_on_t` template on `agrpc::BasicRPC`:
 *
 * @code{cpp}
 * template<auto PrepareAsync>
 * using AwaitableRPC = asio::use_awaitable_t<>::as_default_on_t<agrpc::BasicRPC<PrepareAsync>>;
 * @endcode
 *
 * @tparam PrepareAsync A pointer to the async version of the RPC method. The async version starts with `PrepareAsync`.
 * Or the special marker value `agrpc::CLIENT_GENERIC_UNARY_RPC`/`agrpc::CLIENT_GENERIC_STREAMING_RPC` for generic RPCs.
 * @tparam Executor The executor type, must refer to a `agrpc::GrpcContext`.
 *
 * @since 2.1.0
 */
template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
using RPC = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicRPC<PrepareAsync, Executor>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
