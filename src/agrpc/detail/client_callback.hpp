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

#ifndef AGRPC_DETAIL_CLIENT_CALLBACK_HPP
#define AGRPC_DETAIL_CLIENT_CALLBACK_HPP

#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct ClientUnaryReactorData
{
    detail::ManualResetEvent<void(bool)> initial_metadata_{};
    detail::ManualResetEvent<void(grpc::Status)> finish_{};
};

template <class StubAsync, class Request, class Response>
using AsyncUnaryFn = void (StubAsync::*)(grpc::ClientContext*, const Request*, Response*,
                                         std::function<void(::grpc::Status)>);

template <class CompletionHandler>
class UnaryRequestCallback
{
  private:
    struct DispatchCallback
    {
        //, asio::recycling_allocator<void>());
        using allocator_type = asio::associated_allocator_t<CompletionHandler>;

        void operator()() { std::move(handler_)(std::move(status_)); }

        allocator_type get_allocator() const noexcept { return asio::get_associated_allocator(handler_); }

        CompletionHandler handler_;
        grpc::Status status_;
    };

  public:
    explicit UnaryRequestCallback(CompletionHandler&& handler)
        : handler_(static_cast<CompletionHandler&&>(handler)), work_(asio::make_work_guard(handler_))
    {
    }

    void operator()(grpc::Status status)
    {
        asio::dispatch(work_.get_executor(), DispatchCallback{std::move(handler_), std::move(status)});
    }

  private:
    CompletionHandler handler_;
    decltype(asio::make_work_guard(std::declval<CompletionHandler&>())) work_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CLIENT_CALLBACK_HPP
