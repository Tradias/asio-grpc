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

#ifndef AGRPC_AGRPC_CLIENT_CALL_HPP
#define AGRPC_AGRPC_CLIENT_CALL_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/grpc_executor.hpp>

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

enum class RpcType
{
    CLIENT_UNARY,
    CLIENT_SERVER_STREAMING,
    CLIENT_CLIENT_STREAMING,
    CLIENT_BIDI_STREAMING
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
}

template <auto PrepareAsync, detail::RpcType, class Executor>
class BasicRpcBase;

template <auto PrepareAsync, class Executor, detail::RpcType = detail::RPC_TYPE<PrepareAsync>>
class BasicRpc;

// template <class GrpcStub, class Executor>
// class Stub;

template <class Executor, class Stub, class RequestT, class ResponseT, template <class> class Responder,
          std::unique_ptr<Responder<ResponseT>> (Stub::*PrepareAsync)(grpc::ClientContext*, const RequestT&,
                                                                      grpc::CompletionQueue*)>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_SERVER_STREAMING>
{
  private:
  public:
    using Request = RequestT;
    using Response = ResponseT;
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
    static auto start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
                      const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return BasicRpc(context, executor.executor);
    }

    // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{})
    {
        return true;
    }

    // Reads from the RPC and finishes it if agrpc::read returned `false`.
    // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
    // and the successful end of the stream.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto read(Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }

    [[nodiscard]] grpc::Status& status() noexcept { return status_; }

    [[nodiscard]] executor_type get_executor() const noexcept { return this->executor; }

  private:
    explicit BasicRpc(grpc::ClientContext& context, Executor executor) : context(context), executor(executor) {}

    grpc::ClientContext& context;
    Executor executor;
    // The user must not move the Rpc during reads, stub.call() could allocate memory to avoid that.
    // Would that be helpful?
    std::unique_ptr<Responder<ResponseT>> responder;
    grpc::Status status_;
};

template <class Stub, class RequestT, class ResponseT, template <class> class Responder,
          std::unique_ptr<Responder<RequestT>> (Stub::*PrepareAsync)(grpc::ClientContext*, ResponseT*,
                                                                     grpc::CompletionQueue*),
          class Executor>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_CLIENT_STREAMING>
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

    BasicRpc(Stub&, Executor);

    // Automatically call ClientContext::TryCancel() if not finished
    // ~BasicRpc();

    // Requests the RPC and finishes it if agrpc::request returned `false`.
    // Return immediately if ClientContext.initial_metadata_corked is set.
    // Completes with grpc::Status.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    static auto start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
                      Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{})
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
    auto write(const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

    // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
    // Completes with grpc::Status.
    // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
    template <class CompletionToken = asio::default_completion_token_t<Executor>>
    auto write(const Request& request, grpc::WriteOptions options,
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
    grpc::Status status;
    bool is_writes_done;
};

template <class Stub, class RequestT, class ResponseT, template <class, class> class Responder,
          std::unique_ptr<Responder<RequestT, ResponseT>> (Stub::*PrepareAsync)(grpc::ClientContext*,
                                                                                grpc::CompletionQueue*),
          class Executor>
class BasicRpc<PrepareAsync, Executor, detail::RpcType::CLIENT_BIDI_STREAMING>
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
    static auto start(detail::ExecutorArg<Executor> executor, Stub& stub, grpc::ClientContext& context,
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

#endif  // AGRPC_AGRPC_CLIENT_CALL_HPP
