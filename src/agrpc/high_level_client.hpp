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
 * @brief BasicRPC status base
 */
class BasicRPCStatusBase
{
  public:
    /**
     * @brief The RPC's status code
     */
    [[nodiscard]] grpc::StatusCode status_code() const noexcept { return status_.error_code(); }

    /// Is the status OK?
    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }

    [[nodiscard]] grpc::Status& status() noexcept { return status_; };

    [[nodiscard]] const grpc::Status& status() const noexcept { return status_; };

  private:
    grpc::Status status_;
};

/**
 * @brief BasicRPC executor base
 */
template <class Executor>
class BasicRPCExecutorBase
{
  public:
    /**
     * @brief Executor type
     */
    using executor_type = Executor;

    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

  protected:
    BasicRPCExecutorBase() : executor(agrpc::GrpcExecutor{}) {}

    explicit BasicRPCExecutorBase(const Executor& executor) : executor(executor) {}

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor); }

  private:
    Executor executor;
};

template <class RequestT, template <class> class ResponderT, class Executor>
class BasicRPCClientClientStreamingBase<ResponderT<RequestT>, Executor> : public detail::BasicRPCStatusBase,
                                                                          public detail::BasicRPCExecutorBase<Executor>,
                                                                          private detail::BasicRPCClientContextBase
{
  public:
    using Request = RequestT;

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<BasicRPCClientClientStreamingBase>>(this->grpc_context(),
                                                                                                {}, {*this}, token);
    }

    // // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::WriteClientStreamingSenderImplementation<ResponderT<RequestT>, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, static_cast<CompletionToken&&>(token));
    }

    // // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientFinishSenderImplementation<BasicRPCClientClientStreamingBase>>(
            this->grpc_context(), {}, {*this}, !this->is_finished(), token, this->ok());
    }

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

template <class ResponseT, template <class> class ResponderT, class Executor>
class BasicRPCClientServerStreamingBase<ResponderT<ResponseT>, Executor>
    : public detail::BasicRPCStatusBase,
      public detail::BasicRPCExecutorBase<Executor>,
      private detail::BasicRPCClientContextBase
{
  public:
    using Response = ResponseT;

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<BasicRPCClientServerStreamingBase>>(this->grpc_context(),
                                                                                                {}, {*this}, token);
    }

    // Reads from the RPC and finishes it if agrpc::read returned `false`.
    // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
    // and the successful end of the stream.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadServerStreamingSenderImplementation<ResponderT<ResponseT>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

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
 * @brief BasicRPC client-side bidirectional streaming base
 */
template <class RequestT, class ResponseT, template <class, class> class ResponderT, class Executor>
class BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
    : public detail::BasicRPCStatusBase,
      public detail::BasicRPCExecutorBase<Executor>,
      private detail::BasicRPCClientContextBase
{
  public:
    /**
     * @brief Request type
     */
    using Request = RequestT;
    using Response = ResponseT;

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read_initial_metadata(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitialMetadataSenderImplementation<BasicRPCBidirectionalStreamingBase>>(this->grpc_context(),
                                                                                                 {}, {*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto read(ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientReadBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientWriteBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, static_cast<CompletionToken&&>(token));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto writes_done(CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientWritesDoneSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {}, {*this}, !this->is_writes_done() && !this->is_finished(), token, this->ok());
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto finish(CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientFinishSenderImplementation<BasicRPCBidirectionalStreamingBase>>(
            this->grpc_context(), {}, {*this}, !this->is_finished(), token, this->ok());
    }

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

inline constexpr auto CLIENT_GENERIC_UNARY_RPC = detail::GenericRPCType::CLIENT_UNARY;
inline constexpr auto CLIENT_GENERIC_STREAMING_RPC = detail::GenericRPCType::CLIENT_STREAMING;

/**
 * @brief BasicRPC for client-side unary calls
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::ClientUnaryRequest<StubT, RequestT, ResponderT<ResponseT>> PrepareAsync, class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY> : public detail::BasicRPCStatusBase,
                                                                       public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Stub = StubT;
    using Request = RequestT;
    using Response = ResponseT;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        const RequestT& request, ResponseT& response,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {response}, {grpc_context, stub, context, request}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, const RequestT& request,
                        ResponseT& response, CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, response,
                                 static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;
};

/**
 * @brief BasicRPC for client-side generic unary calls
 */
template <class Executor>
class BasicRPC<detail::GenericRPCType::CLIENT_UNARY, Executor, agrpc::RPCType::CLIENT_UNARY>
    : public detail::BasicRPCStatusBase, public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Stub = grpc::GenericStub;
    using Request = grpc::ByteBuffer;
    using Response = grpc::ByteBuffer;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<detail::GenericRPCType::CLIENT_UNARY, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context, const grpc::ByteBuffer& request, grpc::ByteBuffer& response,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::GenericClientUnaryRequestSenderImplementation<Executor>>(
            grpc_context, {response}, {grpc_context, method, stub, context, request}, token);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(const Executor& executor, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context, const grpc::ByteBuffer& request, grpc::ByteBuffer& response,
                        CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), method, stub, context, request, response,
                                 static_cast<CompletionToken&&>(token));
    }

  private:
    friend detail::GenericClientUnaryRequestSenderImplementation<Executor>;

    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;
};

/**
 * @brief BasicRPC for client-side client streams
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::PrepareAsyncClientClientStreamingRequest<StubT, ResponderT<RequestT>, ResponseT> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>
    : public detail::BasicRPCClientClientStreamingBase<ResponderT<RequestT>, Executor>
{
  public:
    using Stub = StubT;
    using Response = ResponseT;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>;
    };

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    // Return immediately if ClientContext.initial_metadata_corked is set.
    // Completes with grpc::Status.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        ResponseT& response, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, response}, token);
    }

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
 * @brief BasicRPC for client-side server streams
 */
template <class StubT, class RequestT, class ResponseT, template <class> class ResponderT,
          detail::PrepareAsyncClientServerStreamingRequest<StubT, RequestT, ResponderT<ResponseT>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>
    : public detail::BasicRPCClientServerStreamingBase<ResponderT<ResponseT>, Executor>
{
  public:
    using Stub = StubT;
    using Request = RequestT;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;
    };

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        const RequestT& request, CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, request}, token);
    }

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
 * @brief BasicRPC for client-side bidirectional streams
 */
template <class StubT, class RequestT, class ResponseT, template <class, class> class ResponderT,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<StubT, ResponderT<RequestT, ResponseT>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>
    : public detail::BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
{
  public:
    using Stub = StubT;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;
    };

    /**
     * @brief Request
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        CompletionToken token = detail::DefaultCompletionTokenT<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context}, token);
    }

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

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
using RPC = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicRPC<PrepareAsync, Executor>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
