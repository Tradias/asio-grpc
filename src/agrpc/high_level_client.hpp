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
#include <agrpc/detail/high_level_client_sender.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/detail/name.hpp>
#include <agrpc/detail/rpc_client_context_base.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief (experimental) RPC's executor base
 *
 * @since 2.1.0
 */
template <class Executor>
class RPCExecutorBase
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
    [[nodiscard]] const executor_type& get_executor() const noexcept { return executor_; }

  protected:
    RPCExecutorBase() : executor_(agrpc::GrpcExecutor{}) {}

    explicit RPCExecutorBase(const Executor& executor) : executor_(executor) {}

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor_); }

  private:
    Executor executor_;
};
}

/**
 * @brief (experimental) A marker value to RPC for generic unary RPCs
 *
 * @since 2.1.0
 */
inline constexpr auto CLIENT_GENERIC_UNARY_RPC = detail::GenericRPCType::CLIENT_UNARY;

/**
 * @brief (experimental) A marker value to RPC for generic streaming RPCs
 *
 * @since 2.1.0
 */
inline constexpr auto CLIENT_GENERIC_STREAMING_RPC = detail::GenericRPCType::CLIENT_STREAMING;

/**
 * @brief (experimental) I/O object for client-side unary RPCs
 *
 * @tparam UnaryPrepareAsync A pointer to the generated, async version of the RPC method. The async version starts with
 * `PrepareAsync`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * Terminal and partial. Cancellation is performed by invoking
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984).
 * Operations are also cancelled when the deadline of the RPC has been reached (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6)).
 *
 * @since 2.6.0
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::ClientUnaryRequest<StubT, RequestT, ResponderT<ResponseT>> UnaryPrepareAsync, class Executor>
class RPC<UnaryPrepareAsync, Executor>
{
  public:
    static constexpr agrpc::RPCType TYPE = agrpc::RPCType::CLIENT_UNARY;

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
     * @brief Rebind the RPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The RPC type when rebound to the specified executor
         */
        using other = RPC<UnaryPrepareAsync, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equivalent to the generated `Service::service_full_name()`.
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
     *
     * @since 2.6.0
     */
    static constexpr std::string_view service_name() noexcept
    {
        return std::string_view(detail::CLIENT_SERVICE_NAME_V<UnaryPrepareAsync>);
    }

    /**
     * @brief Name of the RPC method
     *
     * E.g. for `agrpc::RPC<&example::Example::Stub::PrepareAsyncMyMethod>` the return value would be `"MyMethod"`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view method_name() noexcept
    {
        return std::string_view(detail::CLIENT_METHOD_NAME_V<UnaryPrepareAsync>);
    }

    /**
     * @brief Start a unary request
     *
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
            detail::ClientUnaryRequestSenderImplementation<UnaryPrepareAsync>>(
            grpc_context, {context, response}, {grpc_context, stub, context, request}, token);
    }

    /**
     * @brief Start a generic unary request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, const RequestT& request,
                        ResponseT& response, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return RPC::request(detail::query_grpc_context(executor), stub, context, request, response,
                            static_cast<CompletionToken&&>(token));
    }
};

/**
 * @brief (experimental) I/O object for client-side generic unary RPCs
 *
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * Terminal and partial. Cancellation is performed by invoking
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984).
 * Operations are also cancelled when the deadline of the RPC has been reached (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6)).
 *
 * @since 2.6.0
 */
template <class Executor>
class RPC<agrpc::CLIENT_GENERIC_UNARY_RPC, Executor>
{
  public:
    static constexpr agrpc::RPCType TYPE = agrpc::RPCType::CLIENT_GENERIC_UNARY;

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
     * @brief Rebind the RPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The RPC type when rebound to the specified executor
         */
        using other = RPC<agrpc::CLIENT_GENERIC_UNARY_RPC, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Returns `"AsyncGenericService"`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view service_name() noexcept { return detail::CLIENT_GENERIC_SERVICE_NAME; }

    /**
     * @brief Name of the RPC method
     *
     * Returns an empty string. This method exists to provide a consistent interface for typed and generic `agrpc::RPC`
     * specializations.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view method_name() noexcept { return detail::CLIENT_GENERIC_METHOD_NAME; }

    /**
     * @brief Start a generic unary request
     *
     * @param method The RPC method to call, e.g. "/test.v1.Test/Unary"
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
        return detail::async_initiate_sender_implementation<detail::ClientGenericUnaryRequestSenderImplementation>(
            grpc_context, {context, response}, {grpc_context, method, stub, context, request}, token);
    }

    /**
     * @brief Start a generic unary request (executor overload)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context, const grpc::ByteBuffer& request, grpc::ByteBuffer& response,
                        CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return RPC::request(detail::query_grpc_context(executor), method, stub, context, request, response,
                            static_cast<CompletionToken&&>(token));
    }
};

/**
 * @brief (experimental) I/O object for client-side client-streaming RPCs
 *
 * @tparam ClientStreamingPrepareAsync A pointer to the generated, async version of the RPC method. The async version
 * starts with `PrepareAsync`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * Terminal and partial. Cancellation is performed by invoking
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984).
 * After successful cancellation no further operations may be started on the RPC (except `finish`). Operations are also
 * cancelled when the deadline of the RPC has been reached (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6)).
 *
 * @since 2.6.0
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::PrepareAsyncClientClientStreamingRequest<StubT, ResponderT<RequestT>, ResponseT>
              ClientStreamingPrepareAsync,
          class Executor>
class RPC<ClientStreamingPrepareAsync, Executor>
    : public detail::RPCExecutorBase<Executor>, public detail::AutoCancelClientContextAndResponder<ResponderT<RequestT>>
{
  private:
    using Responder = ResponderT<RequestT>;

  public:
    static constexpr agrpc::RPCType TYPE = agrpc::RPCType::CLIENT_CLIENT_STREAMING;

    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief The request message type
     */
    using Request = RequestT;

    /**
     * @brief The response message type
     */
    using Response = ResponseT;

    /**
     * @brief Rebind the RPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The RPC type when rebound to the specified executor
         */
        using other = RPC<ClientStreamingPrepareAsync, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equivalent to the generated `Service::service_full_name()`.
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
     *
     * @since 2.6.0
     */
    static constexpr std::string_view service_name() noexcept
    {
        return std::string_view(detail::CLIENT_SERVICE_NAME_V<ClientStreamingPrepareAsync>);
    }

    /**
     * @brief Name of the RPC method
     *
     * E.g. for `agrpc::RPC<&example::Example::Stub::PrepareAsyncMyMethod>` the return value would be `"MyMethod"`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view method_name() noexcept
    {
        return std::string_view(detail::CLIENT_METHOD_NAME_V<ClientStreamingPrepareAsync>);
    }

    /**
     * @brief Construct from a GrpcContext
     */
    explicit RPC(agrpc::GrpcContext& grpc_context) : detail::RPCExecutorBase<Executor>(grpc_context.get_executor()) {}

    /**
     * @brief Construct from a GrpcContext and an init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this RPC's deadline.
     */
    template <class ClientContextInitFunction>
    RPC(agrpc::GrpcContext& grpc_context, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor()),
          detail::AutoCancelClientContextAndResponder<Responder>(
              static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    /**
     * @brief Construct from an executor
     */
    explicit RPC(const Executor& executor) : detail::RPCExecutorBase<Executor>(executor) {}

    /**
     * @brief Construct from an executor and init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this RPC's deadline.
     */
    template <class ClientContextInitFunction>
    RPC(const Executor& executor, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(executor),
          detail::AutoCancelClientContextAndResponder<Responder>(
              static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    using detail::AutoCancelClientContextAndResponder<Responder>::context;

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
     * `void(bool)`. `true` means that the rpc was started successfully. If it is `false`, then call `finish` to obtain
     * the error details.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(StubT& stub, ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientStreamingRequestSenderImplementation<ClientStreamingPrepareAsync, Executor>>(
            this->grpc_context(), {*this, stub, response}, {}, token);
    }

    using detail::AutoCancelClientContextAndResponder<Responder>::cancel;

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
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<detail::ReadInitialMetadataSenderImplementation<RPC>>(
            this->grpc_context(), {}, {*this}, token);
    }

    /**
     * @brief Send a message to the server
     *
     * WriteOptions options is used to set the write options of this message, otherwise identical to:
     * `write(request, token)`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<detail::WriteClientStreamingSenderImplementation<RPC>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    /**
     * @brief Send a message to the server
     *
     * Only one write may be outstanding at any given time. May not be called concurrently with
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
    auto write(const RequestT& request, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return write(request, grpc::WriteOptions{}, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Finish the RPC
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * May not be used concurrently with other operations and may only be called once.
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
     * @arg Attempts to fill in the response parameter that was passed to `RPC::request`.
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(grpc::Status)`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<detail::ClientFinishSenderImplementation<RPC>>(
            this->grpc_context(), {}, {*this}, token);
    }

  private:
    friend detail::ReadInitialMetadataSenderImplementation<RPC>;
    friend detail::WriteClientStreamingSenderImplementation<RPC>;
    friend detail::ClientFinishSenderImplementation<RPC>;
    friend detail::ClientStreamingRequestSenderInitiation<ClientStreamingPrepareAsync, Executor>;
    friend detail::ClientStreamingRequestSenderImplementation<ClientStreamingPrepareAsync, Executor>;
};

namespace detail
{
/**
 * @brief (experimental) Client-side RPC server-streaming base
 *
 * @since 2.6.0
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::PrepareAsyncClientServerStreamingRequest<StubT, RequestT, ResponderT<ResponseT>>
              ServerStreamingPrepareAsync,
          class Executor>
class ClientRPCServerStreamingBase<ServerStreamingPrepareAsync, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::AutoCancelClientContextAndResponder<ResponderT<ResponseT>>
{
  private:
    using Responder = ResponderT<ResponseT>;

  public:
    static constexpr agrpc::RPCType TYPE = agrpc::RPCType::CLIENT_SERVER_STREAMING;

    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief The request message type
     */
    using Request = RequestT;

    /**
     * @brief The response message type
     */
    using Response = ResponseT;

    /**
     * @brief Rebind the RPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The RPC type when rebound to the specified executor
         */
        using other = RPC<ServerStreamingPrepareAsync, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equivalent to the generated `Service::service_full_name()`.
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
     *
     * @since 2.6.0
     */
    static constexpr std::string_view service_name() noexcept
    {
        return std::string_view(detail::CLIENT_SERVICE_NAME_V<ServerStreamingPrepareAsync>);
    }

    /**
     * @brief Name of the RPC method
     *
     * E.g. for `agrpc::RPC<&example::Example::Stub::PrepareAsyncMyMethod>` the return value would be `"MyMethod"`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view method_name() noexcept
    {
        return std::string_view(detail::CLIENT_METHOD_NAME_V<ServerStreamingPrepareAsync>);
    }

    /**
     * @brief Construct from a GrpcContext
     */
    explicit ClientRPCServerStreamingBase(agrpc::GrpcContext& grpc_context)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor())
    {
    }

    /**
     * @brief Construct from a GrpcContext and an init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this RPC's deadline.
     */
    template <class ClientContextInitFunction>
    ClientRPCServerStreamingBase(agrpc::GrpcContext& grpc_context, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor()),
          detail::AutoCancelClientContextAndResponder<Responder>(
              static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    /**
     * @brief Construct from an executor
     */
    explicit ClientRPCServerStreamingBase(const Executor& executor) : detail::RPCExecutorBase<Executor>(executor) {}

    /**
     * @brief Construct from an executor and init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this RPC's deadline.
     */
    template <class ClientContextInitFunction>
    ClientRPCServerStreamingBase(const Executor& executor, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(executor),
          detail::AutoCancelClientContextAndResponder<Responder>(
              static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    using detail::AutoCancelClientContextAndResponder<Responder>::context;

    /**
     * @brief Start a server-streaming request
     *
     * @param stub The Stub that corresponds to the RPC method, e.g. `example::v1::Example::Stub`.
     * @param request The request message, save to delete when this function returns, unless a deferred completion token
     * is used like `agrpc::use_sender` or `asio::deferred`.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the rpc was started successfully. If it is `false`, then call `finish` to obtain
     * the error details.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(StubT& stub, const RequestT& request,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientStreamingRequestSenderImplementation<ServerStreamingPrepareAsync, Executor>>(
            this->grpc_context(), {*this, stub, request}, {}, token);
    }

    using detail::AutoCancelClientContextAndResponder<Responder>::cancel;

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
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<ClientRPCServerStreamingBase>>(this->grpc_context(), {},
                                                                                           {*this}, token);
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
     * stream has failed (or been cancelled).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadServerStreamingSenderImplementation<ClientRPCServerStreamingBase>>(this->grpc_context(),
                                                                                           {response}, {*this}, token);
    }

    /**
     * @brief Finish the RPC
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * May not be used concurrently with other operations and may only be called once.
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
     * completion signature is `void(grpc::Status)`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientFinishServerStreamingSenderImplementation<ClientRPCServerStreamingBase>>(this->grpc_context(),
                                                                                                   {}, {*this}, token);
    }

  private:
    friend detail::ReadInitialMetadataSenderImplementation<ClientRPCServerStreamingBase>;
    friend detail::ReadServerStreamingSenderImplementation<ClientRPCServerStreamingBase>;
    friend detail::ClientFinishServerStreamingSenderImplementation<ClientRPCServerStreamingBase>;
    friend detail::ClientStreamingRequestSenderInitiation<ServerStreamingPrepareAsync, Executor>;
    friend detail::ClientStreamingRequestSenderImplementation<ServerStreamingPrepareAsync, Executor>;
};
}

/**
 * @brief (experimental) I/O object for client-side server-streaming RPCs
 *
 * @tparam ServerStreamingPrepareAsync A pointer to the generated, async version of the RPC method. The async version
 * starts with `PrepareAsync`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * Terminal and partial. Cancellation is performed by invoking
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984).
 * After successful cancellation no further operations may be started on the RPC (except `finish`). Operations are also
 * cancelled when the deadline of the RPC has been reached (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6)).
 *
 * @since 2.6.0
 */
template <class StubT, class RequestT, class ResponseT,
          std::unique_ptr<grpc::ClientAsyncReader<ResponseT>> (StubT::*ServerStreamingPrepareAsync)(
              grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*),
          class Executor>
class RPC<ServerStreamingPrepareAsync, Executor>
    : public detail::ClientRPCServerStreamingBase<ServerStreamingPrepareAsync, Executor>
{
    using detail::ClientRPCServerStreamingBase<ServerStreamingPrepareAsync, Executor>::ClientRPCServerStreamingBase;
};
template <class StubT, class RequestT, class ResponseT,
          std::unique_ptr<grpc::ClientAsyncReaderInterface<ResponseT>> (StubT::*ServerStreamingPrepareAsync)(
              grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*),
          class Executor>
class RPC<ServerStreamingPrepareAsync, Executor>
    : public detail::ClientRPCServerStreamingBase<ServerStreamingPrepareAsync, Executor>
{
    using detail::ClientRPCServerStreamingBase<ServerStreamingPrepareAsync, Executor>::ClientRPCServerStreamingBase;
};

namespace detail
{
/**
 * @brief (experimental) Client-side RPC bidirectional-streaming base
 *
 * @since 2.6.0
 */
template <class RequestT, class ResponseT, template <class, class> class ResponderT, class Executor>
class ClientRPCBidiStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
    : public detail::RPCExecutorBase<Executor>,
      public detail::AutoCancelClientContextAndResponder<ResponderT<RequestT, ResponseT>>
{
  private:
    using Responder = ResponderT<RequestT, ResponseT>;

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
     * @brief Construct from a GrpcContext
     */
    explicit ClientRPCBidiStreamingBase(agrpc::GrpcContext& grpc_context)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor())
    {
    }

    /**
     * @brief Construct from a GrpcContext and an init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this RPC's deadline.
     */
    template <class ClientContextInitFunction>
    ClientRPCBidiStreamingBase(agrpc::GrpcContext& grpc_context, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(grpc_context.get_executor()),
          detail::AutoCancelClientContextAndResponder<Responder>(
              static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    /**
     * @brief Construct from an executor
     */
    explicit ClientRPCBidiStreamingBase(const Executor& executor) : detail::RPCExecutorBase<Executor>(executor) {}

    /**
     * @brief Construct from an executor and init function
     *
     * @tparam ClientContextInitFunction A function with signature `void(grpc::ClientContext&)` which will be invoked
     * during construction. It can, for example, be used to set this RPC's deadline.
     */
    template <class ClientContextInitFunction>
    ClientRPCBidiStreamingBase(const Executor& executor, ClientContextInitFunction&& init_function)
        : detail::RPCExecutorBase<Executor>(executor),
          detail::AutoCancelClientContextAndResponder<Responder>(
              static_cast<ClientContextInitFunction&&>(init_function))
    {
    }

    using detail::AutoCancelClientContextAndResponder<Responder>::context;

    using detail::AutoCancelClientContextAndResponder<Responder>::cancel;

    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the `read()` or `write()`
     * method.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` indicates that the metadata was read. If it is `false`, then the call is dead.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<ClientRPCBidiStreamingBase>>(this->grpc_context(), {},
                                                                                         {*this}, token);
    }

    /**
     * @brief Receive a message from the server
     *
     * This is thread-safe with respect to `write()` or `writes_done()` methods. It should not be called concurrently
     * with other operations. It is not meaningful to call it concurrently with another read on the same stream since
     * reads on the same stream are delivered in order.
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
            detail::ClientReadBidiStreamingSenderImplementation<Responder, Executor>>(this->grpc_context(), {response},
                                                                                      {*this}, token);
    }

    /**
     * @brief Send a message to the server
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to `read()`. It should not
     * be called concurrently with other operations. gRPC does not take ownership or a reference to `request`, so it is
     * safe to to deallocate once write returns (unless a deferred completion token is used like `agrpc::use_sender` or
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
            detail::ClientWriteBidiStreamingSenderImplementation<Responder, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    /**
     * @brief Send a message to the server (default WriteOptions)
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return write(request, grpc::WriteOptions{}, static_cast<CompletionToken&&>(token));
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
        return detail::async_initiate_sender_implementation<
            detail::ClientWritesDoneSenderImplementation<Responder, Executor>>(this->grpc_context(), {}, {*this},
                                                                               token);
    }

    /**
     * @brief Signal WritesDone and finish the RPC
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * May not be used concurrently with other operations and may only be called once.
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
     * completion signature is `void(grpc::Status)`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientFinishSenderImplementation<ClientRPCBidiStreamingBase>>(this->grpc_context(), {}, {*this},
                                                                                  token);
    }

  private:
    friend detail::ReadInitialMetadataSenderImplementation<ClientRPCBidiStreamingBase>;
    friend detail::ClientReadBidiStreamingSenderImplementation<Responder, Executor>;
    friend detail::ClientWriteBidiStreamingSenderImplementation<Responder, Executor>;
    friend detail::ClientWritesDoneSenderImplementation<Responder, Executor>;
    friend detail::ClientFinishSenderImplementation<ClientRPCBidiStreamingBase>;
};
}

/**
 * @brief (experimental) I/O object for client-side bidirectional-streaming RPCs
 *
 * @tparam BidiStreamingPrepareAsync A pointer to the generated, async version of the RPC method. The async version
 * starts with `PrepareAsync`.
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * Terminal and partial. Cancellation is performed by invoking
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984).
 * After successful cancellation no further operations may be started on the RPC (except `finish`). Operations are also
 * cancelled when the deadline of the RPC has been reached (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6)).
 *
 * @since 2.6.0
 */
template <class StubT, class RequestT, class ResponseT, template <class, class> class ResponderT,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<StubT, ResponderT<RequestT, ResponseT>>
              BidiStreamingPrepareAsync,
          class Executor>
class RPC<BidiStreamingPrepareAsync, Executor>
    : public detail::ClientRPCBidiStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
{
  public:
    static constexpr agrpc::RPCType TYPE = agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING;

    /**
     * @brief The stub type
     */
    using Stub = StubT;

    /**
     * @brief Rebind the RPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The RPC type when rebound to the specified executor
         */
        using other = RPC<BidiStreamingPrepareAsync, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Equivalent to the generated `Service::service_full_name()`.
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
     *
     * @since 2.6.0
     */
    static constexpr std::string_view service_name() noexcept
    {
        return std::string_view(detail::CLIENT_SERVICE_NAME_V<BidiStreamingPrepareAsync>);
    }

    /**
     * @brief Name of the RPC method
     *
     * E.g. for `agrpc::RPC<&example::Example::Stub::PrepareAsyncMyMethod>` the return value would be `"MyMethod"`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view method_name() noexcept
    {
        return std::string_view(detail::CLIENT_METHOD_NAME_V<BidiStreamingPrepareAsync>);
    }

    using detail::ClientRPCBidiStreamingBase<ResponderT<RequestT, ResponseT>, Executor>::ClientRPCBidiStreamingBase;

    /**
     * @brief Start a bidirectional-streaming request
     *
     * @param stub The Stub that corresponds to the RPC method, e.g. `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the rpc was started successfully. If it is `false`, then call `finish` to obtain
     * the error details.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(StubT& stub, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientStreamingRequestSenderImplementation<BidiStreamingPrepareAsync, Executor>>(
            this->grpc_context(), {*this, stub}, {}, token);
    }

  private:
    friend detail::ClientStreamingRequestSenderInitiation<BidiStreamingPrepareAsync, Executor>;
    friend detail::ClientStreamingRequestSenderImplementation<BidiStreamingPrepareAsync, Executor>;
};

/**
 * @brief (experimental) I/O object for client-side generic streaming RPCs
 *
 * @tparam Executor The executor type, must be capable of referring to a `agrpc::GrpcContext`.
 *
 * **Per-Operation Cancellation**
 *
 * Terminal and partial. Cancellation is performed by invoking
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984).
 * After successful cancellation no further operations may be started on the RPC (except `finish`). Operations are also
 * cancelled when the deadline of the RPC has been reached (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6)).
 *
 * @since 2.6.0
 */
template <class Executor>
class RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor>
    : public detail::ClientRPCBidiStreamingBase<grpc::GenericClientAsyncReaderWriter, Executor>
{
  public:
    static constexpr agrpc::RPCType TYPE = agrpc::RPCType::CLIENT_GENERIC_STREAMING;

    /**
     * @brief The stub type
     */
    using Stub = grpc::GenericStub;

    /**
     * @brief Rebind the RPC to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The RPC type when rebound to the specified executor
         */
        using other = RPC<agrpc::CLIENT_GENERIC_STREAMING_RPC, OtherExecutor>;
    };

    /**
     * @brief Name of the gRPC service
     *
     * Returns `"AsyncGenericService"`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view service_name() noexcept { return detail::CLIENT_GENERIC_SERVICE_NAME; }

    /**
     * @brief Name of the RPC method
     *
     * Returns an empty string. This method exists to provide a consistent interface between typed and generic
     * specializations of `agrpc::RPC`.
     *
     * @since 2.6.0
     */
    static constexpr std::string_view method_name() noexcept { return detail::CLIENT_GENERIC_METHOD_NAME; }

    using detail::ClientRPCBidiStreamingBase<grpc::GenericClientAsyncReaderWriter,
                                             Executor>::ClientRPCBidiStreamingBase;

    /**
     * @brief Start a generic streaming request
     *
     * @param token A completion token like `asio::yield_context` or `agrpc::use_sender`. The completion signature is
     * `void(bool)`. `true` means that the rpc was started successfully. If it is `false`, then call `finish` to obtain
     * the error details.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto start(const std::string& method, grpc::GenericStub& stub,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientStreamingRequestSenderImplementation<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor>>(
            this->grpc_context(), {*this, method, stub}, {}, token);
    }

  private:
    friend detail::ClientStreamingRequestSenderInitiation<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor>;
    friend detail::ClientStreamingRequestSenderImplementation<agrpc::CLIENT_GENERIC_STREAMING_RPC, Executor>;
};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
