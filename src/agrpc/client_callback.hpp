// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_AGRPC_CLIENT_CALLBACK_HPP
#define AGRPC_AGRPC_CLIENT_CALLBACK_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/client_callback.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/reactor_executor_base.hpp>
#include <grpcpp/support/client_callback.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

// Unary
template <class Executor>
class BasicClientUnaryReactor : private grpc::ClientUnaryReactor, public detail::ReactorExecutorBase<Executor>
{
  public:
    using BasicClientUnaryReactor::ReactorExecutorBase::ReactorExecutorBase;

    [[nodiscard]] grpc::ClientUnaryReactor* get() noexcept { return this; }

    void start() { this->StartCall(); }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    void OnReadInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnDone(const grpc::Status& status) final { data_.finish_.set(grpc::Status{status}); }

    detail::ClientUnaryReactorData data_;
};

using ClientUnaryReactor = BasicClientUnaryReactor<asio::any_io_executor>;

template <class StubAsync, class Request, class Response, class CompletionToken>
auto request(detail::AsyncUnaryFn<StubAsync, Request, Response> fn, StubAsync* stub,
             grpc::ClientContext& client_context, const Request& request, Response& response, CompletionToken&& token)
{
    return asio::async_initiate<CompletionToken, void(grpc::Status)>(
        [fn](auto&& handler, StubAsync* stub, grpc::ClientContext* client_context, const Request* request,
             Response* response)
        {
            (*stub.*fn)(client_context, request, response,
                        detail::UnaryRequestCallback{static_cast<decltype(handler)&&>(handler)});
        },
        token, stub, &client_context, &request, &response);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_CLIENT_CALLBACK_HPP
