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

#ifndef AGRPC_DETAIL_RPCS_HPP
#define AGRPC_DETAIL_RPCS_HPP

#include "agrpc/detail/asioForward.hpp"

#include <grpcpp/grpcpp.h>

#include <utility>

namespace agrpc::detail
{
template <class RPC, class Request, class Responder>
using ServerMultiArgRequest = void (RPC::*)(grpc::ServerContext*, Request*, Responder*, grpc::CompletionQueue*,
                                            grpc::ServerCompletionQueue*, void*);

template <class RPC, class Responder>
using ServerSingleArgRequest = void (RPC::*)(grpc::ServerContext*, Responder*, grpc::CompletionQueue*,
                                             grpc::ServerCompletionQueue*, void*);

template <class RPC, class Request, class Reader>
using ClientUnaryRequest = Reader (RPC::*)(grpc::ClientContext* context, const Request& request,
                                           grpc::CompletionQueue* cq);

template <class RPC, class Request, class Reader>
using ClientServerStreamingRequest = Reader (RPC::*)(grpc::ClientContext* context, const Request& request,
                                                     grpc::CompletionQueue* cq, void* tag);

template <class RPC, class Writer, class Response>
using ClientSideStreamingRequest = Writer (RPC::*)(grpc::ClientContext* context, Response* response,
                                                   grpc::CompletionQueue* cq, void* tag);

template <class RPC, class ReaderWriter>
using ClientBidirectionalStreamingRequest = ReaderWriter (RPC::*)(grpc::ClientContext* context,
                                                                  grpc::CompletionQueue* cq, void* tag);

template <class Responder, class CompletionHandler>
struct CompletionHandlerWithResponder
{
    using executor_type = typename asio::associated_executor<CompletionHandler>::type;

    CompletionHandler completion_handler;
    Responder responder;

    template <class... Args>
    CompletionHandlerWithResponder(CompletionHandler completion_handler, Args&&... args)
        : completion_handler(std::move(completion_handler)), responder(std::forward<Args>(args)...)
    {
    }

    void operator()(bool ok) { completion_handler(std::pair{std::move(responder), ok}); }

    [[nodiscard]] auto get_executor() const noexcept { return asio::get_associated_executor(completion_handler); }
};

template <class Responder, class CompletionHandler, class... Args>
auto make_completion_handler_with_responder(CompletionHandler completion_handler, Args&&... args)
{
    return CompletionHandlerWithResponder<Responder, CompletionHandler>{std::move(completion_handler),
                                                                        std::forward<Args>(args)...};
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_RPCS_HPP
