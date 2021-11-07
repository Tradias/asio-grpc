// Copyright 2021 Dennis Hezel
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
#include "agrpc/detail/initiate.hpp"
#include "agrpc/detail/rpcs.hpp"
#include "agrpc/initiate.hpp"

#include <grpcpp/alarm.h>

namespace agrpc
{
template <class RPCContextImplementationAllocator>
class RPCRequestContext
{
  public:
    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args) const
    {
        return (*impl)(std::forward<Handler>(handler), std::forward<Args>(args)...);
    }

    constexpr auto args() const noexcept { return impl->args(); }

  private:
    friend detail::RPCContextImplementation;

    detail::AllocatedPointer<RPCContextImplementationAllocator> impl;

    constexpr explicit RPCRequestContext(detail::AllocatedPointer<RPCContextImplementationAllocator>&& impl) noexcept
        : impl(std::move(impl))
    {
    }
};

namespace detail
{
struct WaitFn
{
    template <class Deadline, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::Alarm& alarm, const Deadline& deadline, CompletionToken token = {}) const
    {
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if (auto cancellation_slot = asio::get_associated_cancellation_slot(token); cancellation_slot.is_connected())
        {
            cancellation_slot.template emplace<detail::AlarmCancellationHandler>(alarm);
        }
#endif
        return agrpc::grpc_initiate(detail::AlarmInitFunction{alarm, deadline}, std::move(token));
    }
};

struct RequestFn
{
    // Server
    template <class RPC, class Service, class Request, class Responder,
              class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                    grpc::ServerContext& server_context, Request& request, Responder& responder,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            detail::ServerMultiArgRequestInitFunction{rpc, service, server_context, request, responder},
            std::move(token));
    }

    template <class RPC, class Service, class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                    grpc::ServerContext& server_context, Responder& responder, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(detail::ServerSingleArgRequestInitFunction{rpc, service, server_context, responder},
                                    std::move(token));
    }

    // Client
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
    template <class RPC, class Stub, class Request, class Reader, class Executor = asio::any_io_executor>
    auto operator()(detail::ClientUnaryRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request,
                    asio::use_awaitable_t<Executor> token = {}) const ->
        typename asio::async_result<asio::use_awaitable_t<Executor>, void(Reader)>::return_type
    {
        auto* completion_queue = co_await agrpc::get_completion_queue(token);
        co_return(stub.*rpc)(&client_context, request, completion_queue);
    }

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
    template <class RPC, class Stub, class Request, class Reader, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request, CompletionToken token = {}) const
    {
        return detail::grpc_initiate_with_payload<Reader>(
            detail::ClientServerStreamingRequestConvenienceInitFunction{rpc, stub, client_context, request},
            std::move(token));
    }
#endif

    template <class RPC, class Stub, class Request, class Reader, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc, Stub& stub,
                    grpc::ClientContext& client_context, const Request& request, Reader& reader,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            detail::ClientServerStreamingRequestInitFunction{rpc, stub, client_context, request, reader},
            std::move(token));
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class RPC, class Stub, class Writer, class Response,
              class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientSideStreamingRequest<RPC, Writer, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, Response& response, CompletionToken token = {}) const
    {
        return detail::grpc_initiate_with_payload<Writer>(
            detail::ClientSideStreamingRequestConvenienceInitFunction{rpc, stub, client_context, response},
            std::move(token));
    }
#endif

    template <class RPC, class Stub, class Writer, class Response,
              class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientSideStreamingRequest<RPC, Writer, Response> rpc, Stub& stub,
                    grpc::ClientContext& client_context, Writer& writer, Response& response,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            detail::ClientSideStreamingRequestInitFunction{rpc, stub, client_context, writer, response},
            std::move(token));
    }

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class RPC, class Stub, class ReaderWriter, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc, Stub& stub,
                    grpc::ClientContext& client_context, CompletionToken token = {}) const
    {
        return detail::grpc_initiate_with_payload<ReaderWriter>(
            detail::ClientBidirectionalStreamingRequestConvenienceInitFunction{rpc, stub, client_context},
            std::move(token));
    }
#endif

    template <class RPC, class Stub, class ReaderWriter, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc, Stub& stub,
                    grpc::ClientContext& client_context, ReaderWriter& reader_writer, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            detail::ClientBidirectionalStreamingRequestInitFunction{rpc, stub, client_context, reader_writer},
            std::move(token));
    }
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
struct RepeatedlyRequestFn
{
    template <class RPC, class Service, class Request, class Responder, class Handler>
    void operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                    Handler handler) const;

    template <class RPC, class Service, class Responder, class Handler>
    void operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, Handler handler) const;
};
#endif

struct ReadFn
{
    // Server
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, Request& request,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::Read{reader, request},
            std::move(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, Request& request,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Read{reader_writer, request},
            std::move(token));
    }

    // Client
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReader<Response>& reader, Response& response, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ClientAsyncReaderInitFunctions<Response>::Read{reader, response},
                                    std::move(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, Response& response,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Read{reader_writer, response},
            std::move(token));
    }
};

struct WriteFn
{
    // Server
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ServerAsyncWriterInitFunctions<Response>::Write{writer, response},
                                    std::move(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const Response& response, grpc::WriteOptions options,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteWithOptions{writer, response, options},
            std::move(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Write{reader_writer, response},
            std::move(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    grpc::WriteOptions options, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::WriteWithOptions{
                reader_writer, response, options},
            std::move(token));
    }

    // Client
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::Write{writer, request},
                                    std::move(token));
    }

    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, const Request& request, grpc::WriteOptions options,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncWriterInitFunctions<Request>::WriteWithOptions{writer, request, options},
            std::move(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, const Request& request,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Write{reader_writer, request},
            std::move(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, const Request& request,
                    grpc::WriteOptions options, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::WriteWithOptions{
                reader_writer, request, options},
            std::move(token));
    }
};

struct WritesDoneFn
{
    // Client
    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::WritesDone{writer},
                                    std::move(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::WritesDone{reader_writer},
            std::move(token));
    }
};

struct FinishFn
{
    // Server
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& writer, const grpc::Status& status,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ServerAsyncWriterInitFunctions<Response>::Finish{writer, status},
                                    std::move(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, const Response& response,
                    const grpc::Status& status, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::Finish{reader, response, status},
            std::move(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncResponseWriter<Response>& writer, const Response& response,
                    const grpc::Status& status, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncResponseWriterInitFunctions<Response>::Write{writer, response, status},
            std::move(token));
    }

    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const grpc::Status& status,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::Finish{reader_writer, status},
            std::move(token));
    }

    // Client
    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReader<Response>& reader, grpc::Status& status, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ClientAsyncReaderInitFunctions<Response>::Finish{reader, status},
                                    std::move(token));
    }

    template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncWriter<Request>& writer, grpc::Status& status, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(typename detail::ClientAsyncWriterInitFunctions<Request>::Finish{writer, status},
                                    std::move(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncResponseReader<Response>& reader, Response& response, grpc::Status& status,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncResponseReaderInitFunctions<Response>::Finish{reader, response, status},
            std::move(token));
    }

    template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, grpc::Status& status,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ClientAsyncReaderWriterInitFunctions<Request, Response>::Finish{reader_writer, status},
            std::move(token));
    }
};

struct WriteAndFinishFn
{
    // Server
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                    grpc::WriteOptions options, const grpc::Status& status, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderWriterInitFunctions<Response, Request>::WriteAndFinish{
                reader_writer, response, options, status},
            std::move(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncWriter<Response>& reader_writer, const Response& response,
                    grpc::WriteOptions options, const grpc::Status& status, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncWriterInitFunctions<Response>::WriteAndFinish{reader_writer, response, options,
                                                                                      status},
            std::move(token));
    }
};

struct FinishWithErrorFn
{
    // Server
    template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncReader<Response, Request>& reader, const grpc::Status& status,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncReaderInitFunctions<Response, Request>::FinishWithError{reader, status},
            std::move(token));
    }

    template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(grpc::ServerAsyncResponseWriter<Response>& writer, const grpc::Status& status,
                    CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(
            typename detail::ServerAsyncResponseWriterInitFunctions<Response>::FinishWithError{writer, status},
            std::move(token));
    }
};

struct SendInitialMetadataFn
{
    template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Responder& responder, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(detail::SendInitialMetadataInitFunction<Responder>{responder}, std::move(token));
    }
};

struct ReadInitialMetadataFn
{
    template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Responder& responder, CompletionToken token = {}) const
    {
        return agrpc::grpc_initiate(detail::ReadInitialMetadataInitFunction<Responder>{responder}, std::move(token));
    }
};
}  // namespace detail

inline constexpr detail::WaitFn wait{};

inline constexpr detail::RequestFn request{};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
inline constexpr detail::RepeatedlyRequestFn repeatedly_request{};
#endif

inline constexpr detail::ReadFn read{};

inline constexpr detail::WriteFn write{};

inline constexpr detail::WritesDoneFn writes_done{};

inline constexpr detail::FinishFn finish{};

inline constexpr detail::WriteAndFinishFn write_and_finish{};

inline constexpr detail::FinishWithErrorFn finish_with_error{};

inline constexpr detail::SendInitialMetadataFn send_initial_metadata{};

inline constexpr detail::ReadInitialMetadataFn read_initial_metadata{};
}  // namespace agrpc

#endif  // AGRPC_AGRPC_RPCS_HPP

#include "agrpc/detail/repeatedlyRequest.hpp"