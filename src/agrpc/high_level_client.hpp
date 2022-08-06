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
#include <agrpc/detail/high_level_client_sender.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/use_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Executor>
struct ExecutorArg
{
    ExecutorArg(agrpc::GrpcContext& grpc_context) : executor(grpc_context.get_executor()) {}
    ExecutorArg(Executor executor) : executor(executor) {}
    Executor executor;
};

template <auto PrepareAsync>
inline constexpr auto RPC_TYPE = RpcType::CLIENT_UNARY;

template <class Stub, class RequestT, class ResponseT,
          std::unique_ptr<grpc::ClientAsyncReader<ResponseT>> (Stub::*PrepareAsync)(
              grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*)>
inline constexpr auto RPC_TYPE<PrepareAsync> = RpcType::CLIENT_SERVER_STREAMING;

template <class Stub, class RequestT, class ResponseT,
          std::unique_ptr<grpc::ClientAsyncReaderInterface<ResponseT>> (Stub::*PrepareAsync)(
              grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*)>
inline constexpr auto RPC_TYPE<PrepareAsync> = RpcType::CLIENT_SERVER_STREAMING;

template <class Stub, class RequestT, class ResponseT, template <class> class Writer,
          std::unique_ptr<Writer<RequestT>> (Stub::*PrepareAsync)(grpc::ClientContext*, ResponseT*,
                                                                  grpc::CompletionQueue*)>
inline constexpr auto RPC_TYPE<PrepareAsync> = RpcType::CLIENT_CLIENT_STREAMING;

template <class Stub, class RequestT, class ResponseT, template <class, class> class ReaderWriter,
          std::unique_ptr<ReaderWriter<RequestT, ResponseT>> (Stub::*PrepareAsync)(grpc::ClientContext*,
                                                                                   grpc::CompletionQueue*)>
inline constexpr auto RPC_TYPE<PrepareAsync> = RpcType::CLIENT_BIDI_STREAMING;

template <class Sender, class CompletionToken>
auto async_initiate_high_level_sender(Sender&& sender, CompletionToken& token)
{
    return asio::async_initiate<CompletionToken, void(bool)>(
        [&](auto&& completion_handler, auto&& sender)
        {
            using CH = decltype(completion_handler);
            sender.submit(
                detail::CompletionHandlerReceiver<detail::WorkTrackingCompletionHandler<detail::RemoveCrefT<CH>>>(
                    std::forward<CH>(completion_handler)));
        },
        token, std::forward<Sender>(sender));
}

template <class Sender>
decltype(auto) async_initiate_high_level_sender(Sender&& sender, agrpc::UseSender)
{
    return std::forward<Sender>(sender);
}

template <class Implementation, class CompletionToken>
decltype(auto) create_high_level_sender(Implementation&& implementation, CompletionToken& token)
{
    return detail::async_initiate_high_level_sender(detail::BasicGrpcSenderAccess::create(std::move(implementation)),
                                                    token);
}
}

class RpcStatus
{
  public:
    /// Return the instance's error code.
    [[nodiscard]] grpc::StatusCode error_code() const noexcept { return status.error_code(); }

    /// Return the instance's error message.
    [[nodiscard]] std::string error_message() const { return status.error_message(); }

    /// Return the (binary) error details.
    // Usually it contains a serialized google.rpc.Status proto.
    [[nodiscard]] std::string error_details() const { return status.error_details(); }

    /// Is the status OK?
    [[nodiscard]] bool ok() const noexcept { return status.ok(); }

  protected:
    grpc::Status status;
};

template <auto PrepareAsync, class Executor, detail::RpcType = detail::RPC_TYPE<PrepareAsync>>
class BasicRpc;

// template <class GrpcStub, class Executor>
// class Stub;

template <class Executor, class Stub, class RequestT, class ResponseT, template <class> class ResponderT,
          std::unique_ptr<ResponderT<ResponseT>> (Stub::*PrepareAsync)(grpc::ClientContext*, const RequestT&,
                                                                       grpc::CompletionQueue*)>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_SERVER_STREAMING> : public RpcStatus
{
  public:
    using Request = RequestT;
    using Response = ResponseT;
    using Responder = ResponderT<ResponseT>;
    using executor_type = Executor;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRpc<PrepareAsync, OtherExecutor, detail::RpcType::CLIENT_SERVER_STREAMING>;
    };

    // Automatically call ClientContext::TryCancel() if not finished
    // ~BasicRpc() { context.TryCancel(); }
    // Requests the RPC and finishes it if agrpc::request returned `false`.
    // Returns immediately if ClientContext.initial_metadata_corked is set.
    // Completes with some from of `std::expected<Rpc<Rpc, Executor>, grpc::Status>` or
    // should Rpc expose grpc::Status' API?
    // Some RPCs are made without an initial request, so we need another overload, is that intuitive?
    // Maybe this function should be called `unary` and then have variations: `client_streaming` and so on.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static BasicRpc request(Executor executor, Stub& stub, grpc::ClientContext& context, const RequestT& request,
                            CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRpc(context, executor);
    }

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static BasicRpc request(agrpc::GrpcContext& grpc_context, Stub&, grpc::ClientContext& context, const RequestT&,
                            CompletionToken&& = asio::default_completion_token_t<Executor>{})
    {
        return BasicRpc(context, grpc_context.get_executor());
    }

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return detail::create_high_level_sender<detail::ReadInitiateMetadataSenderImplementation<Responder>>(
            {grpc_context(), *responder, this->status}, token);
    }

    // Reads from the RPC and finishes it if agrpc::read returned `false`.
    // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
    // and the successful end of the stream.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read(ResponseT& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    [[nodiscard]] executor_type get_executor() const noexcept { return this->executor; }

    // private:
    explicit BasicRpc(grpc::ClientContext& context, Executor executor) : context(context), executor(executor) {}

    auto& grpc_context() const noexcept { return detail::query_grpc_context(executor); }

    grpc::ClientContext& context;
    Executor executor;
    std::unique_ptr<Responder> responder;
};

template <class Stub, class RequestT, class ResponseT, template <class> class Responder,
          std::unique_ptr<Responder<RequestT>> (Stub::*PrepareAsync)(grpc::ClientContext*, ResponseT*,
                                                                     grpc::CompletionQueue*),
          class Executor>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_CLIENT_STREAMING> : public RpcStatus
{
  public:
    using Request = RequestT;
    using Response = ResponseT;
    using executor_type = Executor;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRpc<PrepareAsync, OtherExecutor, detail::RpcType::CLIENT_CLIENT_STREAMING>;
    };

    // Automatically call ClientContext::TryCancel() if not finished
    // ~BasicRpc();

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    // Return immediately if ClientContext.initial_metadata_corked is set.
    // Completes with grpc::Status.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static BasicRpc start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
                          ResponseT& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRpc(executor.executor);
    }

    // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    // Completes with grpc::Status.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    // Reads from the RPC and finishes it if agrpc::write returned `false`.
    // Completes with grpc::Status.
    // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const RequestT& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
    // Completes with grpc::Status.
    // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const RequestT& request, grpc::WriteOptions options,
               CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
    // Completes with grpc::Status.
    // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto finish(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    [[nodiscard]] executor_type get_executor() const noexcept { return this->executor; }

  private:
    explicit BasicRpc(Executor executor) : executor(executor) {}

    Executor executor;
    std::unique_ptr<Responder<RequestT>> responder;
    bool is_writes_done{};
};

template <class Stub, class RequestT, class ResponseT, template <class, class> class Responder,
          std::unique_ptr<Responder<RequestT, ResponseT>> (Stub::*PrepareAsync)(grpc::ClientContext*,
                                                                                grpc::CompletionQueue*),
          class Executor>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_BIDI_STREAMING> : public RpcStatus
{
  public:
    using Request = RequestT;
    using Response = ResponseT;
    using executor_type = Executor;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRpc<PrepareAsync, OtherExecutor, detail::RpcType::CLIENT_BIDI_STREAMING>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static BasicRpc start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
                          CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRpc(executor.executor);
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return this->executor; }

  private:
    explicit BasicRpc(Executor executor) : executor(executor) {}

    Executor executor;
};

template <class Stub, class RequestT, class ResponseT, template <class> class Responder,
          std::unique_ptr<Responder<ResponseT>> (Stub::*PrepareAsync)(grpc::ClientContext*, const RequestT&,
                                                                      grpc::CompletionQueue*),
          class Executor>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_UNARY> : public RpcStatus
{
  public:
    using Request = RequestT;
    using Response = ResponseT;
    using executor_type = Executor;

    template <class OtherExecutor>
    struct rebind_executor
    {
        using other = BasicRpc<PrepareAsync, OtherExecutor, detail::RpcType::CLIENT_UNARY>;
    };

    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static BasicRpc start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
                          const RequestT& request,
                          CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRpc(executor.executor);
    }

    [[nodiscard]] executor_type get_executor() const noexcept { return this->executor; }

  private:
    explicit BasicRpc(Executor executor) : executor(executor) {}

    Executor executor;
};

template <auto PrepareAsync, class Executor = agrpc::GrpcExecutor>
using Rpc = agrpc::DefaultCompletionToken::as_default_on_t<agrpc::BasicRpc<PrepareAsync, Executor>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_HIGH_LEVEL_CLIENT_HPP
