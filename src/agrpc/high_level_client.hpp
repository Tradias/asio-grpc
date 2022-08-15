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
class BasicRPCStatusBase
{
  public:
    /// Return the instance's error code.
    [[nodiscard]] grpc::StatusCode status_code() const noexcept { return status_.error_code(); }

    /// Is the status OK?
    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }

    [[nodiscard]] grpc::Status& status() noexcept { return status_; };

    [[nodiscard]] const grpc::Status& status() const noexcept { return status_; };

  private:
    grpc::Status status_;
};

template <class Executor>
class BasicRPCExecutorBase
{
  public:
    using executor_type = Executor;

    [[nodiscard]] executor_type get_executor() const noexcept { return executor; }

  protected:
    BasicRPCExecutorBase() : executor(agrpc::GrpcExecutor{}) {}

    explicit BasicRPCExecutorBase(const Executor& executor) : executor(executor) {}

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor); }

  private:
    Executor executor;
};

class BasicRPCClientContextBase
{
  protected:
    BasicRPCClientContextBase() = default;

    BasicRPCClientContextBase(grpc::ClientContext& client_context) : client_context(client_context) {}

    [[nodiscard]] bool is_finished() const noexcept { return !bool{client_context}; }

    void set_finished() noexcept { client_context.release(); }

  private:
    detail::AutoCancelClientContext client_context;
};

template <class RequestT, class ResponseT, class Executor>
class BasicRPCUnaryBase : public detail::BasicRPCStatusBase, public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

  protected:
    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;
};

template <class RequestT, template <class> class ResponderT, class Executor>
class BasicRPCClientClientStreamingBase<ResponderT<RequestT>, Executor> : public detail::BasicRPCStatusBase,
                                                                          public detail::BasicRPCExecutorBase<Executor>,
                                                                          private detail::BasicRPCClientContextBase
{
  public:
    using Request = RequestT;

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitiateMetadataSenderImplementation<BasicRPCClientClientStreamingBase>>(this->grpc_context(),
                                                                                                 {}, {*this}, token);
    }

    // // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::WriteClientStreamingSenderImplementation<ResponderT<RequestT>, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, std::forward<CompletionToken>(token));
    }

    // // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto finish(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientFinishSenderImplementation<BasicRPCClientClientStreamingBase>>(
            this->grpc_context(), {}, {*this}, !this->is_finished(), token, this->ok());
    }

    [[nodiscard]] std::unique_ptr<ResponderT<RequestT>>& responder() noexcept { return responder_; }

  protected:
    friend detail::ReadInitiateMetadataSenderImplementation<BasicRPCClientClientStreamingBase>;
    friend detail::WriteClientStreamingSenderImplementation<ResponderT<RequestT>, Executor>;
    friend detail::ClientFinishSenderImplementation<BasicRPCClientClientStreamingBase>;
    friend detail::BasicRPCAccess;

    BasicRPCClientClientStreamingBase() = default;

    BasicRPCClientClientStreamingBase(const Executor& executor, grpc::ClientContext& client_context)
        : detail::BasicRPCExecutorBase<Executor>(executor), detail::BasicRPCClientContextBase(client_context)
    {
    }

  private:
    std::unique_ptr<ResponderT<RequestT>> responder_;
    bool is_writes_done{};
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
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitiateMetadataSenderImplementation<BasicRPCClientServerStreamingBase>>(this->grpc_context(),
                                                                                                 {}, {*this}, token);
    }

    // Reads from the RPC and finishes it if agrpc::read returned `false`.
    // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
    // and the successful end of the stream.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read(ResponseT& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadServerStreamingSenderImplementation<ResponderT<ResponseT>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

    [[nodiscard]] std::unique_ptr<ResponderT<ResponseT>>& responder() noexcept { return responder_; }

  protected:
    friend detail::ReadInitiateMetadataSenderImplementation<BasicRPCClientServerStreamingBase>;
    friend detail::ReadServerStreamingSenderImplementation<ResponderT<ResponseT>, Executor>;
    friend detail::BasicRPCAccess;

    BasicRPCClientServerStreamingBase() = default;

    BasicRPCClientServerStreamingBase(const Executor& executor, grpc::ClientContext& client_context)
        : detail::BasicRPCExecutorBase<Executor>(executor), detail::BasicRPCClientContextBase(client_context)
    {
    }

  private:
    std::unique_ptr<ResponderT<ResponseT>> responder_;
};

template <class RequestT, class ResponseT, template <class, class> class ResponderT, class Executor>
class BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
    : public detail::BasicRPCStatusBase,
      public detail::BasicRPCExecutorBase<Executor>,
      private detail::BasicRPCClientContextBase
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadInitiateMetadataSenderImplementation<BasicRPCBidirectionalStreamingBase>>(this->grpc_context(),
                                                                                                  {}, {*this}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read(ResponseT& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientReadBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientWriteBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, std::forward<CompletionToken>(token));
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto finish(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_conditional_sender_implementation<
            detail::ClientFinishSenderImplementation<BasicRPCBidirectionalStreamingBase>>(
            this->grpc_context(), {}, {*this}, !this->is_finished(), token, this->ok());
    }

    [[nodiscard]] std::unique_ptr<ResponderT<RequestT, ResponseT>>& responder() noexcept { return responder_; }

  protected:
    friend detail::ReadInitiateMetadataSenderImplementation<BasicRPCBidirectionalStreamingBase>;
    friend detail::ClientReadBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>;
    friend detail::ClientWriteBidiStreamingSenderImplementation<ResponderT<RequestT, ResponseT>, Executor>;
    friend detail::ClientFinishSenderImplementation<BasicRPCBidirectionalStreamingBase>;
    friend detail::BasicRPCAccess;

    BasicRPCBidirectionalStreamingBase() = default;

    BasicRPCBidirectionalStreamingBase(const Executor& executor, grpc::ClientContext& client_context)
        : detail::BasicRPCExecutorBase<Executor>(executor), detail::BasicRPCClientContextBase(client_context)
    {
    }

  private:
    std::unique_ptr<ResponderT<RequestT, ResponseT>> responder_;
    bool is_writes_done{};
};
}

inline constexpr auto CLIENT_GENERIC_UNARY_RPC = detail::GenericRPCType::CLIENT_UNARY;
inline constexpr auto CLIENT_GENERIC_STREAMING_RPC = detail::GenericRPCType::CLIENT_STREAMING;

template <class StubT, class Request, class Response, template <class> class Responder,
          detail::ClientUnaryRequest<StubT, Request, Responder<Response>> PrepareAsync, class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>
    : public detail::BasicRPCUnaryBase<Request, Response, Executor>
{
  public:
    using Stub = StubT;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        const Request& request, Response& response,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {response}, {grpc_context, stub, context, request}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, const Request& request,
                        Response& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, response, token);
    }

  private:
    friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCUnaryBase<Request, Response, Executor>::BasicRPCUnaryBase;
};

template <class Executor>
class BasicRPC<detail::GenericRPCType::CLIENT_UNARY, Executor, agrpc::RPCType::CLIENT_UNARY>
    : public detail::BasicRPCUnaryBase<grpc::ByteBuffer, grpc::ByteBuffer, Executor>
{
  public:
    using Stub = grpc::GenericStub;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<detail::GenericRPCType::CLIENT_UNARY, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, const std::string& method, grpc::GenericStub& stub,
                        grpc::ClientContext& context, const grpc::ByteBuffer& request, grpc::ByteBuffer& response,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::GenericClientUnaryRequestSenderImplementation<Executor>>(
            grpc_context, {response}, {grpc_context, method, stub, context, request}, token);
    }

  private:
    friend detail::GenericClientUnaryRequestSenderImplementation<Executor>;

    using detail::BasicRPCUnaryBase<grpc::ByteBuffer, grpc::ByteBuffer, Executor>::BasicRPCUnaryBase;
};

template <class StubT, class Request, class ResponseT, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<StubT, Responder<Request>, ResponseT> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>
    : public detail::BasicRPCClientClientStreamingBase<Responder<Request>, Executor>
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
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        ResponseT& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, response}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, ResponseT& response,
                        CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, response,
                                 std::forward<CompletionToken>(token));
    }

  private:
    friend detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCClientClientStreamingBase<Responder<Request>, Executor>::BasicRPCClientClientStreamingBase;
};

template <class StubT, class RequestT, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<StubT, RequestT, Responder<Response>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>
    : public detail::BasicRPCClientServerStreamingBase<Responder<Response>, Executor>
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
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        const RequestT& request, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, request}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context, const RequestT& request,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, token);
    }

  private:
    friend detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCClientServerStreamingBase<Responder<Response>, Executor>::BasicRPCClientServerStreamingBase;
};

template <class StubT, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<StubT, Responder<Request, Response>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>
    : public detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>
{
  public:
    using Stub = StubT;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING>;
    };

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, StubT& stub, grpc::ClientContext& context,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, StubT& stub, grpc::ClientContext& context,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, token);
    }

  private:
    friend detail::ClientBidirectionalStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>,
                                                     Executor>::BasicRPCBidirectionalStreamingBase;
};

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
using RPC = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicRPC<PrepareAsync, Executor>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
