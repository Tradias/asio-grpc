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

template <class Deadline, class CompletionToken = agrpc::DefaultCompletionToken>
auto wait(grpc::Alarm& alarm, const Deadline& deadline, CompletionToken token = {})
{
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    if (auto slot = asio::get_associated_cancellation_slot(token); slot.is_connected())
    {
        slot.template emplace<detail::AlarmCancellationHandler>(alarm);
    }
#endif
    return agrpc::grpc_initiate(detail::AlarmFunction{alarm, deadline}, std::move(token));
}

/*
Server
*/
template <class RPC, class Service, class Request, class Responder, class CompletionToken>
auto request(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
             grpc::ServerContext& server_context, Request& request, Responder& responder, CompletionToken token)
{
    return agrpc::grpc_initiate(detail::ServerMultiArgRequestFunction{rpc, service, server_context, request, responder},
                                std::move(token));
}

template <class RPC, class Service, class Responder, class CompletionToken>
auto request(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, grpc::ServerContext& server_context,
             Responder& responder, CompletionToken token)
{
    return agrpc::grpc_initiate(detail::ServerSingleArgRequestFunction{rpc, service, server_context, responder},
                                std::move(token));
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class RPC, class Service, class Request, class Responder, class Handler>
void repeatedly_request(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service, Handler handler)
{
    detail::repeatedly_request(rpc, service, std::move(handler));
}

template <class RPC, class Service, class Responder, class Handler>
void repeatedly_request(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, Handler handler)
{
    detail::repeatedly_request(rpc, service, std::move(handler));
}
#endif

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto read(grpc::ServerAsyncReader<Response, Request>& reader, Request& request, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ServerAsyncReaderFunctions<Response, Request>::Read{reader, request},
                                std::move(token));
}

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto read(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, Request& request, CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncReaderWriterFunctions<Response, Request>::Read{reader_writer, request},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto write(grpc::ServerAsyncWriter<Response>& writer, const Response& response, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ServerAsyncWriterFunctions<Response>::Write{writer, response},
                                std::move(token));
}

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto write(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
           CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncReaderWriterFunctions<Response, Request>::Write{reader_writer, response},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ServerAsyncWriter<Response>& writer, const grpc::Status& status, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ServerAsyncWriterFunctions<Response>::Finish{writer, status},
                                std::move(token));
}

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ServerAsyncReader<Response, Request>& reader, const Response& response, const grpc::Status& status,
            CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncReaderFunctions<Response, Request>::Finish{reader, response, status},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ServerAsyncResponseWriter<Response>& writer, const Response& response, const grpc::Status& status,
            CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncResponseWriterFunctions<Response>::Write{writer, response, status},
        std::move(token));
}

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const grpc::Status& status,
            CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncReaderWriterFunctions<Response, Request>::Finish{reader_writer, status},
        std::move(token));
}

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto write_and_finish(grpc::ServerAsyncReaderWriter<Response, Request>& reader_writer, const Response& response,
                      grpc::WriteOptions options, const grpc::Status& status, CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncReaderWriterFunctions<Response, Request>::WriteAndFinish{reader_writer, response,
                                                                                             options, status},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto write_and_finish(grpc::ServerAsyncWriter<Response>& reader_writer, const Response& response,
                      grpc::WriteOptions options, const grpc::Status& status, CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncWriterFunctions<Response>::WriteAndFinish{reader_writer, response, options, status},
        std::move(token));
}

template <class Response, class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish_with_error(grpc::ServerAsyncReader<Response, Request>& reader, const grpc::Status& status,
                       CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncReaderFunctions<Response, Request>::FinishWithError{reader, status},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish_with_error(grpc::ServerAsyncResponseWriter<Response>& writer, const grpc::Status& status,
                       CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ServerAsyncResponseWriterFunctions<Response>::FinishWithError{writer, status},
        std::move(token));
}

template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
auto send_initial_metadata(Responder& responder, CompletionToken token = {})
{
    return agrpc::grpc_initiate(detail::SendInitialMetadataFunction<Responder>{responder}, std::move(token));
}

/*
Client
*/
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
template <class RPC, class Stub, class Request, class Reader, class Executor = asio::any_io_executor>
auto request(detail::ClientUnaryRequest<RPC, Request, Reader> rpc, Stub& stub, grpc::ClientContext& client_context,
             const Request& request, asio::use_awaitable_t<Executor> token = {}) ->
    typename asio::async_result<asio::use_awaitable_t<Executor>, void(Reader)>::return_type
{
    auto* completion_queue = co_await agrpc::get_completion_queue(token);
    co_return(stub.*rpc)(&client_context, request, completion_queue);
}

template <class RPC, class Stub, class Request, class Reader, class Executor = asio::any_io_executor>
auto request(detail::ClientUnaryRequest<RPC, Request, Reader> rpc, Stub& stub, grpc::ClientContext& client_context,
             const Request& request, Reader& reader, asio::use_awaitable_t<Executor> token = {}) ->
    typename asio::async_result<asio::use_awaitable_t<Executor>, void()>::return_type
{
    auto* completion_queue = co_await agrpc::get_completion_queue(token);
    reader = (stub.*rpc)(&client_context, request, completion_queue);
}
#endif

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class RPC, class Stub, class Request, class Reader, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc, Stub& stub,
             grpc::ClientContext& client_context, const Request& request, CompletionToken token = {})
{
    return detail::grpc_initiate_with_payload<Reader>(
        detail::ClientServerStreamingRequestConvenienceFunction{rpc, stub, client_context, request}, std::move(token));
}
#endif

template <class RPC, class Stub, class Request, class Reader, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ClientServerStreamingRequest<RPC, Request, Reader> rpc, Stub& stub,
             grpc::ClientContext& client_context, const Request& request, Reader& reader, CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        detail::ClientServerStreamingRequestFunction{rpc, stub, client_context, request, reader}, std::move(token));
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class RPC, class Stub, class Writer, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ClientSideStreamingRequest<RPC, Writer, Response> rpc, Stub& stub,
             grpc::ClientContext& client_context, Response& response, CompletionToken token = {})
{
    return detail::grpc_initiate_with_payload<Writer>(
        detail::ClientSideStreamingRequestConvenienceFunction{rpc, stub, client_context, response}, std::move(token));
}
#endif

template <class RPC, class Stub, class Writer, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ClientSideStreamingRequest<RPC, Writer, Response> rpc, Stub& stub,
             grpc::ClientContext& client_context, Writer& writer, Response& response, CompletionToken token = {})
{
    return agrpc::grpc_initiate(detail::ClientSideStreamingRequestFunction{rpc, stub, client_context, writer, response},
                                std::move(token));
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class RPC, class Stub, class ReaderWriter, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc, Stub& stub,
             grpc::ClientContext& client_context, CompletionToken token = {})
{
    return detail::grpc_initiate_with_payload<ReaderWriter>(
        detail::ClientBidirectionalStreamingRequestConvencienceFunction{rpc, stub, client_context}, std::move(token));
}
#endif

template <class RPC, class Stub, class ReaderWriter, class CompletionToken = agrpc::DefaultCompletionToken>
auto request(detail::ClientBidirectionalStreamingRequest<RPC, ReaderWriter> rpc, Stub& stub,
             grpc::ClientContext& client_context, ReaderWriter& reader_writer, CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        detail::ClientBidirectionalStreamingRequestFunction{rpc, stub, client_context, reader_writer},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto read(grpc::ClientAsyncReader<Response>& reader, Response& response, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ClientAsyncReaderFunctions<Response>::Read{reader, response},
                                std::move(token));
}

template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto read(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, Response& response,
          CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ClientAsyncReaderWriterFunctions<Request, Response>::Read{reader_writer, response},
        std::move(token));
}

template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto write(grpc::ClientAsyncWriter<Request>& writer, const Request& request, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ClientAsyncWriterFunctions<Request>::Write{writer, request},
                                std::move(token));
}

template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto write(grpc::ClientAsyncWriter<Request>& writer, const Request& request, grpc::WriteOptions options,
           CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ClientAsyncWriterFunctions<Request>::WriteWithOptions{writer, request, options},
        std::move(token));
}

template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto writes_done(grpc::ClientAsyncWriter<Request>& writer, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ClientAsyncWriterFunctions<Request>::WritesDone{writer},
                                std::move(token));
}

template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto write(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, const Request& request,
           CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ClientAsyncReaderWriterFunctions<Request, Response>::Write{reader_writer, request},
        std::move(token));
}

template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto writes_done(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ClientAsyncReaderWriterFunctions<Request, Response>::WritesDone{reader_writer},
        std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ClientAsyncReader<Response>& reader, grpc::Status& status, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ClientAsyncReaderFunctions<Response>::Finish{reader, status},
                                std::move(token));
}

template <class Request, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ClientAsyncWriter<Request>& writer, grpc::Status& status, CompletionToken token = {})
{
    return agrpc::grpc_initiate(typename detail::ClientAsyncWriterFunctions<Request>::Finish{writer, status},
                                std::move(token));
}

template <class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ClientAsyncResponseReader<Response>& reader, Response& response, grpc::Status& status,
            CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ClientAsyncResponseReaderFunctions<Response>::Finish{reader, response, status},
        std::move(token));
}

template <class Request, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
auto finish(grpc::ClientAsyncReaderWriter<Request, Response>& reader_writer, grpc::Status& status,
            CompletionToken token = {})
{
    return agrpc::grpc_initiate(
        typename detail::ClientAsyncReaderWriterFunctions<Request, Response>::Finish{reader_writer, status},
        std::move(token));
}

template <class Responder, class CompletionToken = agrpc::DefaultCompletionToken>
auto read_initial_metadata(Responder& responder, CompletionToken token = {})
{
    return agrpc::grpc_initiate(detail::ReadInitialMetadataFunction<Responder>{responder}, std::move(token));
}
}  // namespace agrpc

#endif  // AGRPC_AGRPC_RPCS_HPP
