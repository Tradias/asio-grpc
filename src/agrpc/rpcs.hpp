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

#ifndef AGRPC_AGRPC_RPCS_HPP
#define AGRPC_AGRPC_RPCS_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcInitiate.hpp"
#include "agrpc/detail/initiate.hpp"
#include "agrpc/detail/rpcs.hpp"
#include "agrpc/initiate.hpp"

#include <grpcpp/alarm.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct WaitFn
{
    template <class Deadline, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::Alarm& alarm, const Deadline& deadline, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>&&
                     std::is_nothrow_copy_constructible_v<Deadline>)
    {
        return detail::grpc_initiate_with_stop_function<detail::AlarmCancellationHandler>(
            detail::AlarmInitFunction{alarm, deadline}, std::forward<CompletionToken>(token));
    }
};

/**
 * @brief Client and server-side function object to start RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext. The following code would therefore be @b invalid:
 * @code{.cpp}
 * asio::io_context io_context;
 * asio::co_spawn(io_context, []() -> asio::awaitable<void> {
 *   grpc::ServerContext server_context;
 *   grpc::ServerAsyncReader<example::v1::Response, example::v1::Request> reader{&server_context};
 *   // error: asio::this_coro::executor does not refer to a GrpcContext
 *   co_await agrpc::request(&example::v1::Example::AsyncService::RequestClientStreaming, service,
 *                           server_context, reader, asio::use_awaitable);
 * }, asio::detached);
 * @endcode
 * For a safer alternative see `agrpc::AsyncService` and `agrpc::Stub` (work in progress).
 *
 */
struct RequestFn
{
    /**
     * @brief Wait for a unary or server-streaming RPC request from a client
     *
     * Unary RPC:
     *
     * @snippet server.cpp request-unary-server-side
     *
     * Server-streaming RPC:
     *
     * @snippet server.cpp request-server-streaming-server-side
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Request`.
     * @param service The AsyncService that corresponds to the RPC method. In the examples above the service is:
     * `example::v1::Example::AsyncService`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the RPC has indeed been started. If it is `false`
     * then the server has been Shutdown before this particular call got matched to an incoming RPC.
     */
    template <class RPC, class Service, class Request, class Responder,
              class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                    grpc::ServerContext& server_context, Request& request, Responder& responder,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ServerMultiArgRequestInitFunction{rpc, service, server_context, request, responder},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Wait for a client-streaming or bidirectional-streaming RPC request from a client
     *
     * Client-streaming RPC:
     *
     * @snippet server.cpp request-client-streaming-server-side
     *
     * Bidirectional-streaming RPC:
     *
     * @snippet server.cpp request-bidirectional-streaming-server-side
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Request`.
     * @param service The AsyncService that corresponds to the RPC method. In the examples above the service is:
     * `example::v1::Example::AsyncService`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the RPC has indeed been started. If it is `false`
     * then the server has been Shutdown before this particular call got matched to an incoming RPC.
     */
    template <class RPC, class Service, class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                    grpc::ServerContext& server_context, Responder& responder, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ServerSingleArgRequestInitFunction{rpc, service, server_context, responder},
            std::forward<CompletionToken>(token));
    }

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
    /**
     * @brief Convenience function for starting a unary request
     *
     * Example:
     *
     * @snippet client.cpp request-unary-client-side-await
     *
     * @note
     * For better performance use:
     * @snippet client.cpp request-unary-client-side
     * instead.
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     */
    template <class RPC, class Stub, class Request, class Reader, class Executor = asio::any_io_executor>
    auto operator()(detail::ClientUnaryRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request,
                    asio::use_awaitable_t<Executor> token = {}) const ->
        typename asio::async_result<asio::use_awaitable_t<Executor>, void(Reader)>::return_type
    {
        auto* completion_queue = co_await agrpc::get_completion_queue(token);
        co_return(stub.*rpc)(&client_context, request, completion_queue);
    }

    /**
     * @brief Convenience function for starting a unary request
     *
     * Takes `std::unique_ptr<grpc::ClientAsyncResponseReader<Response>>` as an output parameter, otherwise identical
     * to: `operator()(ClientUnaryRequest, Stub&, ClientContext&, const Request&, use_awaitable_t<Executor>)`
     */
    template <class RPC, class Stub, class Request, class Reader, class Executor = asio::any_io_executor>
    auto operator()(detail::ClientUnaryRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request, Reader& reader,
                    asio::use_awaitable_t<Executor> token = {}) const ->
        typename asio::async_result<asio::use_awaitable_t<Executor>, void()>::return_type
    {
        auto* completion_queue = co_await agrpc::get_completion_queue(token);
        reader = (stub.*rpc)(&client_context, request, completion_queue);
    }
#endif

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    /**
     * @brief Convenience function for starting a server-streaming request
     *
     * Sends `std::unique_ptr<grpc::ClientAsyncReader<Response>>` through the completion handler, otherwise
     * identical to `operator()(ClientServerStreamingRequest, Stub&, ClientContext&, const Request&, Reader&,
     * CompletionToken&&)`
     *
     * Example:
     *
     * @snippet client.cpp request-server-streaming-client-side-alt
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(std::pair<std::unique_ptr<grpc::ClientAsyncReader<Response>>, bool>)`. `true`
     * indicates that the RPC is going to go to the wire. If it is `false`, it is not going to the wire. This would
     * happen if the channel is either permanently broken or transiently broken but with the fail-fast option.
     */
    template <class RPC, class Stub, class Request, class Reader, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate_with_payload<Reader>(
            detail::ClientServerStreamingRequestConvenienceInitFunction{rpc, stub, client_context, request},
            std::forward<CompletionToken>(token));
    }
#endif

    /**
     * @brief Start a server-streaming request
     *
     * Example:
     *
     * @snippet client.cpp request-server-streaming-client-side
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the RPC is going to go to the wire. If it is `false`,
     * it is not going to the wire. This would happen if the channel is either permanently broken or transiently broken
     * but with the fail-fast option.
     */
    template <class RPC, class Stub, class Request, class Reader, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request, Reader& reader,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ClientServerStreamingRequestInitFunction{rpc, stub, client_context, request, reader},
            std::forward<CompletionToken>(token));
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    /**
     * @brief Convenience function for starting a client-streaming request
     *
     * Sends `std::unique_ptr<grpc::ClientAsyncWriter<Request>>` through the completion handler, otherwise
     * identical to `operator()(ClientClientStreamingRequest, Stub&, ClientContext&, Writer&, Response&,
     * CompletionToken&&)`
     *
     * Example:
     *
     * @snippet client.cpp request-client-streaming-client-side-alt
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(std::pair<std::unique_ptr<grpc::ClientAsyncWriter<Request>>, bool>)`. `true`
     * indicates that the RPC is going to go to the wire. If it is `false`, it is not going to the wire. This would
     * happen if the channel is either permanently broken or transiently broken but with the fail-fast option.
     */
    template <class RPC, class Stub, class Writer, class Response,
              class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientClientStreamingRequest<RPC, Writer, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, Response& response, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate_with_payload<Writer>(
            detail::ClientClientStreamingRequestConvenienceInitFunction{rpc, stub, client_context, response},
            std::forward<CompletionToken>(token));
    }
#endif

    /**
     * @brief Start a client-streaming request
     *
     * Example:
     *
     * @snippet client.cpp request-client-streaming-client-side
     *
     * @attention Do not use this function with the
     * [initial_metadata_corked](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#af79c64534c7b208594ba8e76021e2696)
     * option set. Call the member function directly instead:
     * @snippet client.cpp request-client-streaming-client-side-corked
     *
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the RPC is going to go to the wire. If it is `false`,
     * it is not going to the wire. This would happen if the channel is either permanently broken or transiently broken
     * but with the fail-fast option.
     */
    template <class RPC, class Stub, class Writer, class Response,
              class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientClientStreamingRequest<RPC, Writer, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, Writer& writer, Response& response,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ClientClientStreamingRequestInitFunction{rpc, stub, client_context, writer, response},
            std::forward<CompletionToken>(token));
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    /**
     * @brief Convenience function for starting a bidirectional-streaming request
     *
     * Sends `std::unique_ptr<grpc::ClientAsyncWriter<Request>>` through the completion handler, otherwise
     * identical to `operator()(ClientClientStreamingRequest, Stub&, ClientContext&, Writer&, Response&,
     * CompletionToken&&)`
     *
     * Example:
     *
     * @snippet client.cpp request-bidirectional-client-side-alt
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(std::pair<std::unique_ptr<grpc::ClientAsyncWriter<Request>>, bool>)`. `true`
     * indicates that the RPC is going to go to the wire. If it is `false`, it is not going to the wire. This would
     * happen if the channel is either permanently broken or transiently broken but with the fail-fast option.
     */
    template <class RPC, class Stub, class ReaderWriter, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc, Stub& stub,
                    grpc::ClientContext& client_context, CompletionToken&& token = {}) const
    {
        return detail::grpc_initiate_with_payload<ReaderWriter>(
            detail::ClientBidirectionalStreamingRequestConvenienceInitFunction{rpc, stub, client_context},
            std::forward<CompletionToken>(token));
    }
#endif

    /**
     * @brief Start a bidirectional-streaming request
     *
     * Example:
     *
     * @snippet client.cpp request-bidirectional-client-side
     *
     * @attention Do not use this function with the
     * [initial_metadata_corked](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#af79c64534c7b208594ba8e76021e2696)
     * option set. Call the member function directly instead:
     * @snippet client.cpp request-client-bidirectional-client-side-corked
     *
     *
     * @param rpc A pointer to the async version of the RPC method. The async version always starts with `Async`.
     * @param stub The Stub that corresponds to the RPC method. In the example above the stub is:
     * `example::v1::Example::Stub`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the RPC is going to go to the wire. If it is `false`,
     * it is not going to the wire. This would happen if the channel is either permanently broken or transiently broken
     * but with the fail-fast option.
     */
    template <class RPC, class Stub, class ReaderWriter, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc, Stub& stub,
                    grpc::ClientContext& client_context, ReaderWriter& reader_writer,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ClientBidirectionalStreamingRequestInitFunction{rpc, stub, client_context, reader_writer},
            std::forward<CompletionToken>(token));
    }
};

struct ReadFn
{
    // Server
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::Read{reader, request},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Read{reader_writer, request},
            std::forward<CompletionToken>(token));
    }

    // Client
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReader<Response>& reader, Response& response, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncReaderInitFunctions<Response>::Read{reader, response},
                                     std::forward<CompletionToken>(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, Response& response,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Read{reader_writer, response},
            std::forward<CompletionToken>(token));
    }
};

struct WriteFn
{
    // Server
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ServerAsyncWriterInitFunctions<Response>::Write{writer, response},
                                     std::forward<CompletionToken>(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response, grpc::WriteOptions options,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteWithOptions{writer, response, options},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Write{reader_writer, response},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    grpc::WriteOptions options, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::WriteWithOptions{
                reader_writer, response, options},
            std::forward<CompletionToken>(token));
    }

    // Client
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::Write{writer, request},
                                     std::forward<CompletionToken>(token));
    }

    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request, grpc::WriteOptions options,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncWriterInitFunctions<Request>::WriteWithOptions{writer, request, options},
            std::forward<CompletionToken>(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, const Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Write{reader_writer, request},
            std::forward<CompletionToken>(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, const Request& request,
                    grpc::WriteOptions options, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::WriteWithOptions{
                reader_writer, request, options},
            std::forward<CompletionToken>(token));
    }
};

struct WritesDoneFn
{
    // Client
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::WritesDone{writer},
                                     std::forward<CompletionToken>(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::WritesDone{reader_writer},
            std::forward<CompletionToken>(token));
    }
};

struct FinishFn
{
    // Server
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ServerAsyncWriterInitFunctions<Response>::Finish{writer, status},
                                     std::forward<CompletionToken>(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, const Response& response,
                    const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::Finish{reader, response, status},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncResponseWriter<Response>& writer, const Response& response,
                    const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncResponseWriterInitFunctions<Response>::Finish{writer, response, status},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Finish{reader_writer, status},
            std::forward<CompletionToken>(token));
    }

    // Client
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReader<Response>& reader, grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncReaderInitFunctions<Response>::Finish{reader, status},
                                     std::forward<CompletionToken>(token));
    }

    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::Finish{writer, status},
                                     std::forward<CompletionToken>(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncResponseReader<Response>& reader, Response& response, grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncResponseReaderInitFunctions<Response>::Finish{reader, response, status},
            std::forward<CompletionToken>(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Finish{reader_writer, status},
            std::forward<CompletionToken>(token));
    }
};

struct WriteAndFinishFn
{
    // Server
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    grpc::WriteOptions options, const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::WriteAndFinish{
                reader_writer, response, options, status},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& reader_writer, const Response& response,
                    grpc::WriteOptions options, const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteAndFinish{reader_writer, response, options,
                                                                                      status},
            std::forward<CompletionToken>(token));
    }
};

struct FinishWithErrorFn
{
    // Server
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::FinishWithError{reader, status},
            std::forward<CompletionToken>(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncResponseWriter<Response>& writer, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncResponseWriterInitFunctions<Response>::FinishWithError{writer, status},
            std::forward<CompletionToken>(token));
    }
};

struct SendInitialMetadataFn
{
    template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Responder& responder, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(detail::SendInitialMetadataInitFunction<Responder>{responder},
                                     std::forward<CompletionToken>(token));
    }
};

struct ReadInitialMetadataFn
{
    template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Responder& responder, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(detail::ReadInitialMetadataInitFunction<Responder>{responder},
                                     std::forward<CompletionToken>(token));
    }
};
}  // namespace detail

inline constexpr detail::WaitFn wait{};

/**
 * @brief Start a new RPC
 *
 * @link detail::RequestFn
 * Client and server-side function to start RPCs.
 * @endlink
 */
inline constexpr detail::RequestFn request{};

inline constexpr detail::ReadFn read{};

inline constexpr detail::WriteFn write{};

inline constexpr detail::WritesDoneFn writes_done{};

inline constexpr detail::FinishFn finish{};

inline constexpr detail::WriteAndFinishFn write_and_finish{};

inline constexpr detail::FinishWithErrorFn finish_with_error{};

inline constexpr detail::SendInitialMetadataFn send_initial_metadata{};

inline constexpr detail::ReadInitialMetadataFn read_initial_metadata{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_RPCS_HPP
