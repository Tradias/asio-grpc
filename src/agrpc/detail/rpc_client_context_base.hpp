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

#ifndef AGRPC_DETAIL_RPC_CLIENT_CONTEXT_BASE_HPP
#define AGRPC_DETAIL_RPC_CLIENT_CONTEXT_BASE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/tagged_ptr.hpp>
#include <grpcpp/client_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Responder>
class AutoCancelClientContextAndResponder
{
  protected:
    AutoCancelClientContextAndResponder() = default;

    template <class ClientContextInitFunction>
    explicit AutoCancelClientContextAndResponder(ClientContextInitFunction&& init_function) noexcept
    {
        static_cast<ClientContextInitFunction&&>(init_function)(client_context_);
    }

    AutoCancelClientContextAndResponder(const AutoCancelClientContextAndResponder&) = delete;

    AutoCancelClientContextAndResponder(AutoCancelClientContextAndResponder&& other) = delete;

    ~AutoCancelClientContextAndResponder() noexcept
    {
        if (!is_finished())
        {
            client_context_.TryCancel();
        }
        if (auto* const responder_ptr = responder_.get())
        {
            std::default_delete<Responder>{}(responder_ptr);
        }
    }

    AutoCancelClientContextAndResponder& operator=(const AutoCancelClientContextAndResponder&) = delete;

    AutoCancelClientContextAndResponder& operator=(AutoCancelClientContextAndResponder&& other) = delete;

    void cancel() noexcept { client_context_.TryCancel(); }

    [[nodiscard]] Responder& responder() noexcept { return *responder_; }

    void set_responder(std::unique_ptr<Responder> responder) noexcept { responder_.set(responder.release()); }

    [[nodiscard]] grpc::ClientContext& context() { return client_context_; }

    [[nodiscard]] bool is_finished() const noexcept { return responder_.template has_bit<0>(); }

    void set_finished() noexcept { responder_.template set_bit<0>(); }

    [[nodiscard]] bool is_writes_done() const noexcept { return responder_.template has_bit<1>(); }

    void set_writes_done() noexcept { responder_.template set_bit<1>(); }

  private:
    friend detail::RPCCancellationFunction;

    grpc::ClientContext client_context_{};
    detail::AtomicTaggedPtr<Responder> responder_{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPC_CLIENT_CONTEXT_BASE_HPP
