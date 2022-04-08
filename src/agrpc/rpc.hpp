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

#ifndef AGRPC_AGRPC_RPC_HPP
#define AGRPC_AGRPC_RPC_HPP

#include "agrpc/defaultCompletionToken.hpp"
#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcInitiate.hpp"
#include "agrpc/detail/rpc.hpp"
#include "agrpc/getCompletionQueue.hpp"

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include "agrpc/detail/initiate.hpp"
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Client and server-side function object to start RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. gRPC does not support cancellation of requests.
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
            detail::ServerMultiArgRequestInitFunction<RPC, Service, Request, Responder>{rpc, service, server_context,
                                                                                        request, responder},
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
            detail::ServerSingleArgRequestInitFunction<RPC, Service, Responder>{rpc, service, server_context,
                                                                                responder},
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
    template <class Stub, class Request, class Response, class Executor = asio::any_io_executor>
    auto operator()(detail::ClientUnaryRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request,
                    asio::use_awaitable_t<Executor> token = {}) const ->
        typename asio::async_result<asio::use_awaitable_t<Executor>,
                                    void(std::unique_ptr<grpc::ClientAsyncResponseReader<Response>>)>::return_type
    {
        auto* completion_queue = co_await agrpc::get_completion_queue(token);
        co_return (stub.*rpc)(&client_context, request, completion_queue);
    }

    /**
     * @brief Convenience function for starting a unary request
     *
     * Takes `std::unique_ptr<grpc::ClientAsyncResponseReader<Response>>` as an output parameter, otherwise identical
     * to: `operator()(ClientUnaryRequest, Stub&, ClientContext&, const Request&, use_awaitable_t<Executor>)`
     */
    template <class Stub, class Request, class Response, class Executor = asio::any_io_executor>
    auto operator()(detail::ClientUnaryRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request,
                    std::unique_ptr<grpc::ClientAsyncResponseReader<Response>>& reader,
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
    template <class Stub, class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientServerStreamingRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate_with_payload<std::unique_ptr<grpc::ClientAsyncReader<Response>>>(
            detail::ClientServerStreamingRequestConvenienceInitFunction<Stub, Request, Response>{
                rpc, stub, client_context, request},
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
    template <class Stub, class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientServerStreamingRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request,
                    std::unique_ptr<grpc::ClientAsyncReader<Response>>& reader, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ClientServerStreamingRequestInitFunction<Stub, Request, Response>{rpc, stub, client_context,
                                                                                      request, reader},
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
    template <class Stub, class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientClientStreamingRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, Response& response, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate_with_payload<std::unique_ptr<grpc::ClientAsyncWriter<Request>>>(
            detail::ClientClientStreamingRequestConvenienceInitFunction<Stub, Request, Response>{
                rpc, stub, client_context, response},
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
    template <class Stub, class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientClientStreamingRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, std::unique_ptr<grpc::ClientAsyncWriter<Request>>& writer,
                    Response& response, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ClientClientStreamingRequestInitFunction<Stub, Request, Response>{rpc, stub, client_context, writer,
                                                                                      response},
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
    template <class Stub, class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientBidirectionalStreamingRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, CompletionToken&& token = {}) const
    {
        return detail::grpc_initiate_with_payload<std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Response>>>(
            detail::ClientBidirectionalStreamingRequestConvenienceInitFunction<Stub, Request, Response>{rpc, stub,
                                                                                                        client_context},
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
    template <class Stub, class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientBidirectionalStreamingRequest<Stub, Request, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context,
                    std::unique_ptr<grpc::ClientAsyncReaderWriter<Request, Response>>& reader_writer,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ClientBidirectionalStreamingRequestInitFunction<Stub, Request, Response>{rpc, stub, client_context,
                                                                                             reader_writer},
            std::forward<CompletionToken>(token));
    }
};

/**
 * @brief Client and server-side function object to read from streaming RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct ReadFn
{
    /**
     * @brief Read from a client stream
     *
     * It should not be called concurrently with other streaming APIs on the same stream. It is not meaningful to call
     * it concurrently with another read on the same stream since reads on the same stream are delivered in order.
     *
     * Example:
     *
     * @snippet server.cpp read-client-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that a valid message was read. If not,
     * you know that there are certainly no more messages that can ever be read from this stream. This could happen
     * because the client has done a WritesDone already.
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::Read{reader, request},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Read from a bidirectional stream (server-side)
     *
     * This is thread-safe with respect to write or writes_done methods on the same stream. It should not be called
     * concurrently with another read on the same stream as the order of delivery will not be defined.
     *
     * Example:
     *
     * @snippet server.cpp read-bidirectional-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that a valid message was read. `false` when
     * there will be no more incoming messages, either because the other side has called WritesDone() or the stream has
     * failed (or been cancelled).
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Read{reader_writer, request},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Read from a server stream
     *
     * It should not be called concurrently with other streaming APIs on the same stream. It is not meaningful to call
     * it concurrently with another read on the same stream since reads on the same stream are delivered in order.
     *
     * Example:
     *
     * @snippet client.cpp read-server-streaming-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that a valid message was read, `false` when the call is
     * dead.
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReader<Response>& reader, Response& response, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncReaderInitFunctions<Response>::Read{reader, response},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Read from a bidirectional stream (client-side)
     *
     * This is thread-safe with respect to write or writes_done methods. It should not be called concurrently with other
     * streaming APIs on the same stream. It is not meaningful to call it concurrently with another read on the same
     * stream since reads on the same stream are delivered in order.
     *
     * Example:
     *
     * @snippet client.cpp read-bidirectional-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that a valid message was read, `false` when the call is
     * dead.
     */
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

/**
 * @brief Client and server-side function object to write to streaming RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct WriteFn
{
    /**
     * @brief Write to a server stream
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to read. gRPC does not
     * take ownership or a reference to `response`, so it is safe to to deallocate once write returns.
     *
     * Example:
     *
     * @snippet server.cpp write-server-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ServerAsyncWriterInitFunctions<Response>::Write{writer, response},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Write to a server stream
     *
     * WriteOptions options is used to set the write options of this message, otherwise identical to:
     * `operator()(ServerAsyncWriter&, const Response&, CompletionToken&&)`
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response, grpc::WriteOptions options,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteWithOptions{writer, response, options},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Write to a bidirectional stream (server-side)
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to read. gRPC does not
     * take ownership or a reference to `response`, so it is safe to to deallocate once write returns.
     *
     * Example:
     *
     * @snippet server.cpp write-bidirectional-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Write{reader_writer, response},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Write to a bidirectional stream (server-side)
     *
     * WriteOptions options is used to set the write options of this message, otherwise identical to:
     * `operator()(ServerAsyncReaderWriter&, const Response&, CompletionToken&&)`
     */
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

    /**
     * @brief Write to a client stream
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to read. gRPC does not
     * take ownership or a reference to `request`, so it is safe to to deallocate once write returns.
     *
     * Example:
     *
     * @snippet client.cpp write-client-streaming-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::Write{writer, request},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Write to a client stream
     *
     * WriteOptions options is used to set the write options of this message, otherwise identical to:
     * `operator()(ClientAsyncWriter&, const Request&, CompletionToken&&)`
     */
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request, grpc::WriteOptions options,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncWriterInitFunctions<Request>::WriteWithOptions{writer, request, options},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Write to a bidirectional stream (client-side)
     *
     * Only one write may be outstanding at any given time. This is thread-safe with respect to read. gRPC does not
     * take ownership or a reference to `request`, so it is safe to to deallocate once write returns.
     *
     * Example:
     *
     * @snippet client.cpp write-bidirectional-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, const Request& request,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Write{reader_writer, request},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Write to a bidirectional stream (client-side)
     *
     * WriteOptions options is used to set the write options of this message, otherwise identical to:
     * `operator()(ClientAsyncReaderWriter&, const Request&, CompletionToken&&)`
     */
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

/**
 * @brief Client-side function object to signal WritesDone to streaming RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct WritesDoneFn
{
    /**
     * @brief Signal WritesDone to a client stream
     *
     * Signal the client is done with the writes (half-close the client stream). Thread-safe with respect to read.
     *
     * Example:
     *
     * @snippet client.cpp writes_done-client-streaming-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::WritesDone{writer},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Signal WritesDone to a bidirectional client stream
     *
     * Signal the client is done with the writes (half-close the client stream). Thread-safe with respect to read.
     *
     * Example:
     *
     * @snippet client.cpp write_done-bidirectional-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::WritesDone{reader_writer},
            std::forward<CompletionToken>(token));
    }
};

/**
 * @brief Client and server-side function object to finish RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct FinishFn
{
    /**
     * @brief Finish a server stream (server-side)
     *
     * Indicate that the stream is to be finished with a certain status code. Should not be used concurrently with other
     * operations.
     *
     * It is appropriate to call this method when either:
     *
     * @arg All messages from the client have been received (either known implictly, or explicitly because a previous
     * read operation returned `false`).
     * @arg It is desired to end the call early with some non-OK status code.
     *
     * This operation will end when the server has finished sending out initial metadata (if not sent already) and
     * status, or if some failure occurred when trying to do so.
     *
     * The ServerContext associated with the call is used for sending trailing (and initial if not already
     * sent) metadata to the client. There are no restrictions to the code of status, it may be non-OK. gRPC does not
     * take ownership or a reference to status, so it is safe to to deallocate once finish returns.
     *
     * Example:
     *
     * @snippet server.cpp finish-server-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ServerAsyncWriterInitFunctions<Response>::Finish{writer, status},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a client stream (server-side)
     *
     * Side effect:
     *
     * @arg Also sends initial metadata if not alreay sent.
     * @arg Uses the ServerContext associated with the call to send possible initial and trailing metadata.
     *
     * @note Response is not sent if status has a non-OK code.
     *
     * gRPC does not take ownership or a reference to response and status, so it is safe to deallocate once finish
     * returns.
     *
     * Example:
     *
     * @snippet server.cpp finish-client-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, const Response& response,
                    const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::Finish{reader, response, status},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a unary RPC (server-side)
     *
     * Indicate that the RPC is to be finished and request notification when the server has sent the appropriate
     * signals to the client to end the call. Should not be used concurrently with other operations.
     *
     * Side effect:
     *
     * @arg Also sends initial metadata if not already sent (using the ServerContext associated with the call).
     *
     * @note If status has a non-OK code, then response will not be sent, and the client will receive only the status
     * with possible trailing metadata.
     *
     * Example:
     *
     * @snippet server.cpp finish-unary-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncResponseWriter<Response>& writer, const Response& response,
                    const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncResponseWriterInitFunctions<Response>::Finish{writer, response, status},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a bidirectional stream (server-side)
     *
     * Indicate that the stream is to be finished with a certain status code. Should not be used concurrently with other
     * operations.
     *
     * It is appropriate to call this method when either:
     *
     * @arg All messages from the client have been received (either known implictly, or explicitly because a previous
     * read operation returned `false`).
     * @arg It is desired to end the call early with some non-OK status code.
     *
     * This operation will end when the server has finished sending out initial metadata (if not sent already) and
     * status, or if some failure occurred when trying to do so.
     *
     * The ServerContext associated with the call is used for sending trailing (and initial if not
     * already sent) metadata to the client. There are no restrictions to the code of status, it may be non-OK. gRPC
     * does not take ownership or a reference to status, so it is safe to to deallocate once finish returns.
     *
     * Example:
     *
     * @snippet server.cpp finish-bidirectional-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Finish{reader_writer, status},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a server stream (client-side)
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * Should not be used concurrently with other operations.
     *
     * It is appropriate to call this method exactly once when:
     *
     * @arg All messages from the server have been received (either known implictly, or explicitly because a previous
     * read operation returned `false`).
     *
     * The operation will finish when either:
     *
     * @arg All incoming messages have been read and the server has returned a status.
     * @arg The server has returned a non-OK status.
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
     * Example:
     *
     * @snippet client.cpp finish-server-streaming-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool should always be `true`.
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReader<Response>& reader, grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncReaderInitFunctions<Response>::Finish{reader, status},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a client stream (client-side)
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * Should not be used concurrently with other operations.
     *
     * It is appropriate to call this method exactly once when:
     *
     * @arg The client side has no more message to send (this can be declared implicitly by calling this method, or
     * explicitly through an earlier call to the writes_done method).
     *
     * The operation will finish when either:
     *
     * @arg All incoming messages have been read and the server has returned a status.
     * @arg The server has returned a non-OK status.
     * @arg The call failed for some reason and the library generated a status.
     *
     * Note that implementations of this method attempt to receive initial metadata from the server if initial metadata
     * has not been received yet.
     *
     * Side effect:
     *
     * @arg The ClientContext associated with the call is updated with possible initial and trailing metadata received
     * from the server.
     * @arg Attempts to fill in the response parameter that was passed to `request`.
     *
     * Example:
     *
     * @snippet client.cpp finish-client-streaming-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool should always be `true`.
     */
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::Finish{writer, status},
                                     std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a unary RPC (client-side)
     *
     * Receive the server's response message and final status for the call.
     *
     * This operation will finish when either:
     *
     * @arg The server's response message and status have been received.
     * @arg The server has returned a non-OK status (no message expected in this case).
     * @arg The call failed for some reason and the library generated a non-OK status.
     *
     * Side effect:
     *
     * @arg The ClientContext associated with the call is updated with possible initial and trailing metadata sent from
     * the server.
     *
     * Example:
     *
     * @snippet client.cpp finish-unary-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool should always be `true`.
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncResponseReader<Response>& reader, Response& response, grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncResponseReaderInitFunctions<Response>::Finish{reader, response, status},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a bidirectional stream (client-side)
     *
     * Indicate that the stream is to be finished and request notification for when the call has been ended.
     *
     * Should not be used concurrently with other operations.
     *
     * It is appropriate to call this method exactly once when:
     *
     * @arg All messages from the server have been received (either known implictly, or explicitly because a previous
     * read operation returned `false`).
     * @arg The client side has no more message to send (this can be declared implicitly by calling this method, or
     * explicitly through an earlier call to the writes_done method).
     *
     * The operation will finish when either:
     *
     * @arg All incoming messages have been read and the server has returned a status.
     * @arg The server has returned a non-OK status.
     * @arg The call failed for some reason and the library generated a status.
     *
     * Note that implementations of this method attempt to receive initial metadata from the server if initial metadata
     * has not been received yet.
     *
     * Side effect:
     *
     * @arg The ClientContext associated with the call is updated with possible initial and trailing metadata sent from
     * the server.
     *
     * Example:
     *
     * @snippet client.cpp finish-bidirectional-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool should always be `true`.
     */
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

/**
 * @brief Function object to coalesce write and send trailing metadata of streaming RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct WriteLastFn
{
    /**
     * @brief Coalesce write and send trailing metadata of a server stream
     *
     * `write_last` buffers the response. The writing of response is held
     * until `finish` is called, where response and trailing metadata are coalesced
     * and write is initiated. Note that `write_last` can only buffer response up to
     * the flow control window size. If response size is larger than the window
     * size, it will be sent on wire without buffering.
     *
     * gRPC does not take ownership or a reference to response, so it is safe to
     * to deallocate once `write_last` returns.
     *
     * Example:
     *
     * @snippet server.cpp write_last-server-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response, grpc::WriteOptions options,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteLast{writer, response, options},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Perform `write` and `writes_done` in a single step
     *
     * gRPC does not take ownership or a reference to response, so it is safe to
     * to deallocate once `write_last` returns.
     *
     * Example:
     *
     * @snippet client.cpp write_last-client-streaming-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request, grpc::WriteOptions options,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncWriterInitFunctions<Request>::WriteLast{writer, request, options},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Coalesce write and send trailing metadata of a server stream
     *
     * `write_last` buffers the response. The writing of response is held
     * until `finish` is called, where response and trailing metadata are coalesced
     * and write is initiated. Note that `write_last` can only buffer response up to
     * the flow control window size. If response size is larger than the window
     * size, it will be sent on wire without buffering.
     *
     * gRPC does not take ownership or a reference to response, so it is safe to
     * to deallocate once `write_last` returns.
     *
     * Example:
     *
     * @snippet server.cpp write_last-bidirectional-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    grpc::WriteOptions options, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::WriteLast{reader_writer, response,
                                                                                                options},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Perform `write` and `writes_done` in a single step
     *
     * gRPC does not take ownership or a reference to response, so it is safe to
     * to deallocate once `write_last` returns.
     *
     * Example:
     *
     * @snippet client.cpp write_last-bidirectional-client-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& writer, const Request& request,
                    grpc::WriteOptions options, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::WriteLast{writer, request,
                                                                                                options},
            std::forward<CompletionToken>(token));
    }
};

/**
 * @brief Server-side function object to coalesce write and finish of streaming RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct WriteAndFinishFn
{
    /**
     * @brief Coalesce write and finish of a server stream
     *
     * Write response and coalesce it with trailing metadata which contains status, using WriteOptions
     * options.
     *
     * write_and_finish is equivalent of performing write_last and finish in a single step.
     *
     * gRPC does not take ownership or a reference to response and status, so it is safe to deallocate once
     * write_and_finish returns.
     *
     * Implicit input parameter:
     *
     * @arg The ServerContext associated with the call is used for sending trailing (and initial) metadata to the
     * client.
     *
     * @note Status must have an OK code.
     *
     * Example:
     *
     * @snippet server.cpp write_and_finish-server-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response, grpc::WriteOptions options,
                    const grpc::Status& status, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteAndFinish{writer, response, options,
                                                                                      status},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Coalesce write and finish of a bidirectional stream
     *
     * Write response and coalesce it with trailing metadata which contains status, using WriteOptions
     * options.
     *
     * write_and_finish is equivalent of performing write_last and finish in a single step.
     *
     * gRPC does not take ownership or a reference to response and status, so it is safe to deallocate once
     * write_and_finish returns.
     *
     * Implicit input parameter:
     *
     * @arg The ServerContext associated with the call is used for sending trailing (and initial) metadata to the
     * client.
     *
     * @note Status must have an OK code.
     *
     * Example:
     *
     * @snippet server.cpp write_and_finish-bidirectional-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
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
};

/**
 * @brief Server-side function object to finish RPCs with an error
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct FinishWithErrorFn
{
    /**
     * @brief Finish a client stream with an error
     *
     * It should not be called concurrently with other streaming APIs on the same stream.
     *
     * Side effect:
     *
     * @arg Sends initial metadata if not alreay sent.
     * @arg Uses the ServerContext associated with the call to send possible initial and trailing metadata.
     *
     * gRPC does not take ownership or a reference to status, so it is safe to deallocate once finish_with_error
     * returns.
     *
     * @note Status must have a non-OK code.
     *
     * Example:
     *
     * @snippet server.cpp finish_with_error-client-streaming-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool should always be `true`.
     */
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, const grpc::Status& status,
                    CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::FinishWithError{reader, status},
            std::forward<CompletionToken>(token));
    }

    /**
     * @brief Finish a unary RPC with an error
     *
     * Indicate that the stream is to be finished with a non-OK status, and request notification for when the server has
     * finished sending the appropriate signals to the client to end the call.
     *
     * Should not be used concurrently with other operations.
     *
     * Side effect:
     *
     * @arg Sends initial metadata if not already sent (using the ServerContext associated with this call).
     *
     * gRPC does not take ownership or a reference to status, so it is safe to deallocate once finish_with_error
     * returns.
     *
     * @note Status must have a non-OK code.
     *
     * Example:
     *
     * @snippet server.cpp finish_with_error-unary-server-side
     *
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. The bool should always be `true`.
     */
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

/**
 * @brief Server-side function object to send initial metadata for RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct SendInitialMetadataFn
{
    /**
     * @brief Send initial metadata
     *
     * Request notification of the sending of initial metadata to the client.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the Finish method.
     *
     * Example:
     *
     * @snippet server.cpp send_initial_metadata-unary-server-side
     *
     * @param responder `grpc::ServerAsyncResponseWriter`, `grpc::ServerAsyncReader`, `grpc::ServerAsyncWriter` or
     * `grpc::ServerAsyncReaderWriter`
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` means that the data/metadata/status/etc is going to go to the wire.
     * If it is `false`, it is not going to the wire because the call is already dead (i.e., canceled, deadline expired,
     * other side dropped the channel, etc).
     */
    template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Responder& responder, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(detail::SendInitialMetadataInitFunction<Responder>{responder},
                                     std::forward<CompletionToken>(token));
    }
};

/**
 * @brief Client-side function object to read initial metadata for RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct ReadInitialMetadataFn
{
    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the read method.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * Example:
     *
     * @snippet client.cpp read_initial_metadata-unary-client-side
     *
     * @param responder `grpc::ClientAsyncResponseReader`, `grpc::ClientAsyncReader`, `grpc::ClientAsyncWriter` or
     * `grpc::ClientAsyncReaderWriter`
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the metadata was read, `false` when the call is
     * dead.
     */
    template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Responder& responder, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(detail::ReadInitialMetadataInitFunction<Responder>{responder},
                                     std::forward<CompletionToken>(token));
    }
};
}  // namespace detail

/**
 * @brief Start a new RPC
 *
 * @link detail::RequestFn
 * Client and server-side function to start RPCs.
 * @endlink
 */
inline constexpr detail::RequestFn request{};

/**
 * @brief Read from a streaming RPC
 *
 * @link detail::ReadFn
 * Client and server-side function to read from streaming RPCs.
 * @endlink
 */
inline constexpr detail::ReadFn read{};

/**
 * @brief Write to a streaming RPC
 *
 * @link detail::WriteFn
 * Client and server-side function to write to streaming RPCs.
 * @endlink
 */
inline constexpr detail::WriteFn write{};

/**
 * @brief Signal WritesDone to a streaming RPC
 *
 * @link detail::WritesDoneFn
 * Client-side function to signal WritesDone to streaming RPCs.
 * @endlink
 */
inline constexpr detail::WritesDoneFn writes_done{};

/**
 * @brief Finish a RPC
 *
 * @link detail::FinishFn
 * Client and server-side function to finish RPCs.
 * @endlink
 */
inline constexpr detail::FinishFn finish{};

/**
 * @brief Coalesce write and send trailing metadata of a streaming RPC
 *
 * @link detail::WriteLastFn
 * Client and server-side function to coalesce write and send trailing metadata of streaming RPCs.
 * @endlink
 */
inline constexpr detail::WriteLastFn write_last{};

/**
 * @brief Coalesce write and finish of a streaming RPC
 *
 * @link detail::WriteAndFinishFn
 * Server-side function to coalesce write and finish of streaming RPCs.
 * @endlink
 */
inline constexpr detail::WriteAndFinishFn write_and_finish{};

/**
 * @brief Finish a RPC with an error
 *
 * @link detail::FinishWithErrorFn
 * Server-side function to finish RPCs with an error.
 * @endlink
 */
inline constexpr detail::FinishWithErrorFn finish_with_error{};

/**
 * @brief Send initial metadata for a RPC
 *
 * @link detail::SendInitialMetadataFn
 * Server-side function to send initial metadata for RPCs.
 * @endlink
 */
inline constexpr detail::SendInitialMetadataFn send_initial_metadata{};

/**
 * @brief Read initial metadata for a RPC
 *
 * @link detail::ReadInitialMetadataFn
 * Client-side function to read initial metadata for RPCs.
 * @endlink
 */
inline constexpr detail::ReadInitialMetadataFn read_initial_metadata{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_RPC_HPP
