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
    [[nodiscard]] grpc::StatusCode error_code() const noexcept { return status_.error_code(); }

    /// Return the instance's error message.
    [[nodiscard]] std::string error_message() const { return status_.error_message(); }

    /// Return the (binary) error details.
    // Usually it contains a serialized google.rpc.Status proto.
    [[nodiscard]] std::string error_details() const { return status_.error_details(); }

    /// Is the status OK?
    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }

  protected:
    grpc::Status& status() noexcept { return status_; };

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

template <class RequestT, class ResponseT, class Executor>
class BasicRPCUnaryBase : public detail::BasicRPCStatusBase, public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

  protected:
    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;
};

template <class RequestT, class ResponseT, template <class> class ResponderT, class Executor>
class BasicRPCClientClientStreamingBase<ResponseT, ResponderT<RequestT>, Executor>
    : public detail::BasicRPCStatusBase, public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

    std::unique_ptr<ResponderT<RequestT>>& responder() noexcept { return responder_; }

  protected:
    friend detail::ReadInitiateMetadataSenderImplementation<BasicRPCClientClientStreamingBase>;
    friend detail::WriteClientStreamingSenderImplementation<ResponderT<RequestT>, ResponseT, Executor>;

    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;

    std::unique_ptr<ResponderT<RequestT>> responder_;
};

template <class RequestT, class ResponseT, template <class> class ResponderT, class Executor>
class BasicRPCClientServerStreamingBase<RequestT, ResponderT<ResponseT>, Executor>
    : public detail::BasicRPCStatusBase, public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

    std::unique_ptr<ResponderT<ResponseT>>& responder() noexcept { return responder_; }

  protected:
    friend detail::ReadInitiateMetadataSenderImplementation<BasicRPCClientServerStreamingBase>;
    friend detail::ReadServerStreamingSenderImplementation<RequestT, ResponderT<ResponseT>, Executor>;

    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;

    std::unique_ptr<ResponderT<ResponseT>> responder_;
};

template <class RequestT, class ResponseT, template <class, class> class ResponderT, class Executor>
class BasicRPCBidirectionalStreamingBase<ResponderT<RequestT, ResponseT>, Executor>
    : public detail::BasicRPCStatusBase, public detail::BasicRPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

    std::unique_ptr<ResponderT<RequestT, ResponseT>>& responder() noexcept { return responder_; }

  protected:
    friend detail::ReadInitiateMetadataSenderImplementation<BasicRPCBidirectionalStreamingBase>;

    using detail::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;

    std::unique_ptr<ResponderT<RequestT, ResponseT>> responder_;
};
}

static constexpr auto GENERIC_UNARY_RPC = detail::GenericRPCType::UNARY;
static constexpr auto GENERIC_STREAMING_RPC = detail::GenericRPCType::STREAMING;

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::ClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync, class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>
    : public detail::BasicRPCUnaryBase<Request, Response, Executor>
{
  public:
    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, Stub& stub, grpc::ClientContext& context,
                        const Request& request, Response& response,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {response}, {grpc_context, stub, context, request}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, Stub& stub, grpc::ClientContext& context, const Request& request,
                        Response& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, response, token);
    }

  private:
    friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCUnaryBase<Request, Response, Executor>::BasicRPCUnaryBase;
};

template <class Executor>
class BasicRPC<detail::GenericRPCType::UNARY, Executor, agrpc::RPCType::CLIENT_UNARY>
    : public detail::BasicRPCUnaryBase<grpc::ByteBuffer, grpc::ByteBuffer, Executor>
{
  public:
    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<detail::GenericRPCType::UNARY, OtherExecutor, agrpc::RPCType::CLIENT_UNARY>;
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

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>
    : public detail::BasicRPCClientClientStreamingBase<Response, Responder<Request>, Executor>
{
  private:
    using Base = detail::BasicRPCClientClientStreamingBase<Response, Responder<Request>, Executor>;

  public:
    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>;
    };

    // Automatically call ClientContext::TryCancel() if not finished
    // ~BasicRPC();

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    // Return immediately if ClientContext.initial_metadata_corked is set.
    // Completes with grpc::Status.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, Stub& stub, grpc::ClientContext& context, Response& response,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, response}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, Stub& stub, grpc::ClientContext& context, Response& response,
                        CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, response,
                                 std::forward<CompletionToken>(token));
    }

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<detail::ReadInitiateMetadataSenderImplementation<Base>>(
            this->grpc_context(), {}, {*this}, token);
    }

    // // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const Request& request, grpc::WriteOptions options,
               CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::WriteClientStreamingSenderImplementation<Responder<Request>, Response, Executor>>(
            this->grpc_context(), {request, options}, {*this}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return this->write(request, grpc::WriteOptions{}, std::forward<CompletionToken>(token));
    }

    // // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    // template <class CompletionToken = asio::default_completion_token_t<Executor>>
    // auto finish(CompletionToken token = asio::default_completion_token_t<Executor>{});

  private:
    friend detail::ClientClientStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCClientClientStreamingBase<Response, Responder<Request>,
                                                    Executor>::BasicRPCClientClientStreamingBase;
};

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>
    : public detail::BasicRPCClientServerStreamingBase<Request, Responder<Response>, Executor>
{
  private:
    using Base = detail::BasicRPCClientServerStreamingBase<Request, Responder<Response>, Executor>;

  public:
    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_SERVER_STREAMING>;
    };

    // Automatically call ClientContext::TryCancel() if not finished
    // ~BasicRPC() { context.TryCancel(); }

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(agrpc::GrpcContext& grpc_context, Stub& stub, grpc::ClientContext& context,
                        const Request& request, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>>(
            grpc_context, {}, {grpc_context, stub, context, request}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, Stub& stub, grpc::ClientContext& context, const Request& request,
                        CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, token);
    }

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<detail::ReadInitiateMetadataSenderImplementation<Base>>(
            this->grpc_context(), {}, {*this}, token);
    }

    // Reads from the RPC and finishes it if agrpc::read returned `false`.
    // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
    // and the successful end of the stream.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read(Response& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<
            detail::ReadServerStreamingSenderImplementation<Request, Responder<Response>, Executor>>(
            this->grpc_context(), {response}, {*this}, token);
    }

  private:
    friend detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCClientServerStreamingBase<Request, Responder<Response>,
                                                    Executor>::BasicRPCClientServerStreamingBase;
};

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDI_STREAMING>
    : public detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>
{
  private:
    using Base = detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>, Executor>;

  public:
    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_BIDI_STREAMING>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return detail::async_initiate_sender_implementation<detail::ReadInitiateMetadataSenderImplementation<Base>>(
            this->grpc_context(), {}, {*this}, token);
    }

  private:
    // friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using detail::BasicRPCBidirectionalStreamingBase<Responder<Request, Response>,
                                                     Executor>::BasicRPCBidirectionalStreamingBase;

    std::unique_ptr<Responder<Request, Response>> responder;
};

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
using RPC = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicRPC<PrepareAsync, Executor>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
