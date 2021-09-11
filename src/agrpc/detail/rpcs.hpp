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

#include <grpcpp/alarm.h>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

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
using ClientUnaryRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*);

template <class RPC, class Request, class Reader>
using ClientServerStreamingRequest = Reader (RPC::*)(grpc::ClientContext*, const Request&, grpc::CompletionQueue*,
                                                     void*);

template <class RPC, class Writer, class Response>
using ClientSideStreamingRequest = Writer (RPC::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*, void*);

template <class RPC, class ReaderWriter>
using ClientBidirectionalStreamingRequest = ReaderWriter (RPC::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

template <class Responder, class CompletionHandler>
struct CompletionHandlerWithResponder
{
    using executor_type = asio::associated_executor_t<CompletionHandler>;

    CompletionHandler completion_handler;
    Responder responder;

    template <class... Args>
    CompletionHandlerWithResponder(CompletionHandler completion_handler, Args&&... args)
        : completion_handler(std::move(completion_handler)), responder(std::forward<Args>(args)...)
    {
    }

    void operator()(bool ok) { this->completion_handler(std::pair{std::move(this->responder), ok}); }

    [[nodiscard]] executor_type get_executor() const noexcept
    {
        return asio::get_associated_executor(this->completion_handler);
    }
};

template <class Responder, class CompletionHandler, class... Args>
auto make_completion_handler_with_responder(CompletionHandler completion_handler, Args&&... args)
{
    return detail::CompletionHandlerWithResponder<Responder, CompletionHandler>{std::move(completion_handler),
                                                                                std::forward<Args>(args)...};
}

#if (BOOST_VERSION >= 107700)
struct AlarmCancellationHandler
{
    grpc::Alarm& alarm;

    constexpr explicit AlarmCancellationHandler(grpc::Alarm& alarm) noexcept : alarm(alarm) {}

    void operator()(asio::cancellation_type type)
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            alarm.Cancel();
        }
    }
};
#endif
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_RPCS_HPP
