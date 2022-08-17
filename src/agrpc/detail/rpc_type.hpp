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

#ifndef AGRPC_DETAIL_RPC_TYPE_HPP
#define AGRPC_DETAIL_RPC_TYPE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/rpc_type.hpp>
#include <grpcpp/support/async_stream.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Stub, class Request, class Responder>
using ClientUnaryRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*, const Request&,
                                                                grpc::CompletionQueue*);

template <class Stub, class Request, class Responder>
using AsyncClientServerStreamingRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*, const Request&,
                                                                               grpc::CompletionQueue*, void*);

template <class Stub, class Request, class Responder>
using PrepareAsyncClientServerStreamingRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*,
                                                                                      const Request&,
                                                                                      grpc::CompletionQueue*);

template <class Stub, class Responder, class Response>
using AsyncClientClientStreamingRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*, Response*,
                                                                               grpc::CompletionQueue*, void*);

template <class Stub, class Responder, class Response>
using PrepareAsyncClientClientStreamingRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*, Response*,
                                                                                      grpc::CompletionQueue*);

template <class Stub, class Responder>
using AsyncClientBidirectionalStreamingRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*,
                                                                                      grpc::CompletionQueue*, void*);

template <class Stub, class Responder>
using PrepareAsyncClientBidirectionalStreamingRequest = std::unique_ptr<Responder> (Stub::*)(grpc::ClientContext*,
                                                                                             grpc::CompletionQueue*);

template <class Service, class Request, class Responder>
using ServerMultiArgRequest = void (Service::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                                grpc::ServerCompletionQueue*, void*);

template <class Service, class Responder>
using ServerSingleArgRequest = void (Service::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                                 grpc::ServerCompletionQueue*, void*);

enum class GenericRPCType
{
    CLIENT_UNARY,
    CLIENT_STREAMING
};

// Extra `class = void` exists only to make the Intellisense happy
template <auto PrepareAsync, class = void>
inline constexpr auto RPC_TYPE = agrpc::RPCType::CLIENT_UNARY;

template <class Stub, class Request, class Response,
          std::unique_ptr<grpc::ClientAsyncReader<Response>> (Stub::*PrepareAsync)(grpc::ClientContext*, const Request&,
                                                                                   grpc::CompletionQueue*)>
inline constexpr auto RPC_TYPE<PrepareAsync, std::void_t<decltype(PrepareAsync)>> =
    agrpc::RPCType::CLIENT_SERVER_STREAMING;

template <class Stub, class Request, class Response,
          std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>> (Stub::*PrepareAsync)(
              grpc::ClientContext*, const Request&, grpc::CompletionQueue*)>
inline constexpr auto RPC_TYPE<PrepareAsync, std::void_t<decltype(PrepareAsync)>> =
    agrpc::RPCType::CLIENT_SERVER_STREAMING;

template <class Stub, class Request, class Response, template <class> class Writer,
          detail::PrepareAsyncClientClientStreamingRequest<Stub, Writer<Request>, Response> PrepareAsync>
inline constexpr auto RPC_TYPE<PrepareAsync, std::void_t<decltype(PrepareAsync)>> =
    agrpc::RPCType::CLIENT_CLIENT_STREAMING;

template <class Stub, class Request, class Response, template <class, class> class ReaderWriter,
          detail::PrepareAsyncClientBidirectionalStreamingRequest<Stub, ReaderWriter<Request, Response>> PrepareAsync>
inline constexpr auto RPC_TYPE<PrepareAsync, std::void_t<decltype(PrepareAsync)>> =
    agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING;

template <>
inline constexpr auto RPC_TYPE<detail::GenericRPCType::CLIENT_STREAMING> =
    agrpc::RPCType::CLIENT_BIDIRECTIONAL_STREAMING;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPC_TYPE_HPP
