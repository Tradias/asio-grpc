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
#include <agrpc/detail/completion_handler_receiver.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_sender.hpp>
#include <agrpc/detail/high_level_client_sender.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/use_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Sender, class CompletionToken>
auto async_initiate_sender(Sender&& sender, CompletionToken& token)
{
    return asio::async_initiate<CompletionToken, typename detail::RemoveCrefT<Sender>::Signature>(
        [&](auto&& completion_handler, auto&& sender)
        {
            using CH = decltype(completion_handler);
            std::move(sender).submit(
                detail::CompletionHandlerReceiver<detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CH>>>(
                    std::forward<CH>(completion_handler)));
        },
        token, std::forward<Sender>(sender));
}

template <class Sender>
auto async_initiate_sender(Sender&& sender, agrpc::UseSender)
{
    return std::forward<Sender>(sender);
}

template <class Implementation, class CompletionToken>
auto async_initiate_sender_implementation(agrpc::GrpcContext& grpc_context, Implementation&& implementation,
                                          CompletionToken& token)
{
    return detail::async_initiate_sender(
        detail::BasicGrpcSenderAccess::create(grpc_context, std::forward<Implementation>(implementation)), token);
}
}

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
class BasicRPCBase : public agrpc::BasicRPCStatusBase, public agrpc::BasicRPCExecutorBase<Executor>
{
  public:
    using Request = RequestT;
    using Response = ResponseT;

  protected:
    using agrpc::BasicRPCExecutorBase<Executor>::BasicRPCExecutorBase;
};

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::ClientUnaryRequest<Stub, Request, Responder<Response>> PrepareAsync, class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_UNARY>
    : public agrpc::BasicRPCBase<Request, Response, Executor>
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
            grpc_context, {grpc_context, stub, context, request, response}, token);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto request(const Executor& executor, Stub& stub, grpc::ClientContext& context, const Request& request,
                        Response& response, CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRPC::request(detail::query_grpc_context(executor), stub, context, request, response, token);
    }

  private:
    friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using agrpc::BasicRPCBase<Request, Response, Executor>::BasicRPCBase;
};

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Responder<Request>, Response> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_CLIENT_STREAMING>
    : public agrpc::BasicRPCBase<Request, Response, Executor>
{
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
    // template <class CompletionToken = asio::default_completion_token_t<Executor>>
    // static BasicRPC start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
    //                       Response& response, CompletionToken token =
    //                       asio::default_completion_token_t<Executor>{})
    // {
    //     return BasicRPC(executor.executor);
    // }

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        auto& grpc_context_ = this->grpc_context();
        return detail::async_initiate_sender_implementation<
            detail::ReadInitiateMetadataSenderImplementation<Responder<Request>>>(
            grpc_context_, {grpc_context_, *responder, this->status()}, token);
    }

    // // Reads from the RPC and finishes it if agrpc::write returned `false`.
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    // template <class CompletionToken = asio::default_completion_token_t<Executor>>
    // auto write(const Request& request, CompletionToken token = asio::default_completion_token_t<Executor>{});

    // // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    // template <class CompletionToken = asio::default_completion_token_t<Executor>>
    // auto write(const Request& request, grpc::WriteOptions options,
    //            CompletionToken token = asio::default_completion_token_t<Executor>{});

    // // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
    // // Completes with grpc::Status.
    // // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    // template <class CompletionToken = asio::default_completion_token_t<Executor>>
    // auto finish(CompletionToken token = asio::default_completion_token_t<Executor>{});

  private:
    // friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using agrpc::BasicRPCBase<Request, Response, Executor>::BasicRPCBase;

    std::unique_ptr<Responder<Request>> responder;
};

template <class Stub, class Request, class Response, template <class> class Responder,
          detail::PrepareAsyncClientServerStreamingRequest<Stub, Request, Responder<Response>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_SERVER_STREAMING>
    : public agrpc::BasicRPCBase<Request, Response, Executor>
{
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
            grpc_context, {grpc_context, stub, context, request}, token);
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
        auto& grpc_context_ = this->grpc_context();
        return detail::async_initiate_sender_implementation<
            detail::ReadInitiateMetadataSenderImplementation<Responder<Response>>>(
            grpc_context_, {grpc_context_, *responder, this->status()}, token);
    }

    // Reads from the RPC and finishes it if agrpc::read returned `false`.
    // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
    // and the successful end of the stream.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read(Response& response, CompletionToken token = asio::default_completion_token_t<Executor>{});

  private:
    friend detail::ClientServerStreamingRequestSenderImplementation<PrepareAsync, Executor>;

    using agrpc::BasicRPCBase<Request, Response, Executor>::BasicRPCBase;

    std::unique_ptr<Responder<Response>> responder;
};

template <class Stub, class Request, class Response, template <class, class> class Responder,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, Responder<Request, Response>> PrepareAsync,
          class Executor>
class BasicRPC<PrepareAsync, Executor, agrpc::RPCType::CLIENT_BIDI_STREAMING>
    : public agrpc::BasicRPCBase<Request, Response, Executor>
{
  public:
    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRPC<PrepareAsync, OtherExecutor, agrpc::RPCType::CLIENT_BIDI_STREAMING>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken token = asio::default_completion_token_t<Executor>{})
    {
        auto& grpc_context_ = this->grpc_context();
        return detail::async_initiate_sender_implementation<
            detail::ReadInitiateMetadataSenderImplementation<Responder<Request, Response>>>(
            grpc_context_, {grpc_context_, *responder, this->status()}, token);
    }

  private:
    // friend detail::ClientUnaryRequestSenderImplementation<PrepareAsync, Executor>;

    using agrpc::BasicRPCBase<Request, Response, Executor>::BasicRPCBase;

    std::unique_ptr<Responder<Request, Response>> responder;
};

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
using RPC = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicRPC<PrepareAsync, Executor>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
