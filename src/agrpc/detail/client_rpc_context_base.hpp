// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_CLIENT_RPC_CONTEXT_BASE_HPP
#define AGRPC_DETAIL_CLIENT_RPC_CONTEXT_BASE_HPP

#include <agrpc/detail/forward.hpp>
#include <grpcpp/client_context.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief ClientRPC grpc::ClientContext base
 *
 * @since 2.6.0
 */
template <class Responder>
class ClientRPCContextBase
{
  public:
    /**
     * @brief Get the underlying `grpc::ClientContext`
     */
    [[nodiscard]] grpc::ClientContext& context() { return client_context_; }

    /**
     * @brief Get the underlying `grpc::ClientContext` (const overload)
     */
    [[nodiscard]] const grpc::ClientContext& context() const { return client_context_; }

    /**
     * @brief Cancel this RPC
     *
     * Effectively calls `context().TryCancel()`.
     *
     * Thread-safe
     */
    void cancel() noexcept { client_context_.TryCancel(); }

  protected:
    ClientRPCContextBase() = default;

    template <class ClientContextInitFunction>
    explicit ClientRPCContextBase(ClientContextInitFunction&& init_function) noexcept
    {
        static_cast<ClientContextInitFunction&&>(init_function)(client_context_);
    }

    ClientRPCContextBase(const ClientRPCContextBase&) = delete;

    ClientRPCContextBase(ClientRPCContextBase&& other) = delete;

    ~ClientRPCContextBase() noexcept;

    ClientRPCContextBase& operator=(const ClientRPCContextBase&) = delete;

    ClientRPCContextBase& operator=(ClientRPCContextBase&& other) = delete;

  private:
    friend detail::ClientRPCContextBaseAccess;

    grpc::ClientContext client_context_{};
    Responder* responder_{};
    bool is_finished_{};
    bool is_writes_done_{};
};

struct ClientRPCContextBaseAccess
{
    template <class Responder>
    static Responder& responder(ClientRPCContextBase<Responder>& rpc) noexcept
    {
        return *rpc.responder_;
    }

    template <class Responder>
    static void set_responder(ClientRPCContextBase<Responder>& rpc, std::unique_ptr<Responder> responder) noexcept
    {
        rpc.responder_ = responder.release();
    }

    template <class Responder>
    [[nodiscard]] static bool is_finished(ClientRPCContextBase<Responder>& rpc) noexcept
    {
        return rpc.is_finished_;
    }

    template <class Responder>
    static void set_finished(ClientRPCContextBase<Responder>& rpc) noexcept
    {
        rpc.is_finished_ = true;
    }

    template <class Responder>
    [[nodiscard]] static bool is_writes_done(ClientRPCContextBase<Responder>& rpc) noexcept
    {
        return rpc.is_writes_done_;
    }

    template <class Responder>
    static void set_writes_done(ClientRPCContextBase<Responder>& rpc, bool done) noexcept
    {
        rpc.is_writes_done_ = done;
    }
};

template <class Responder>
inline ClientRPCContextBase<Responder>::~ClientRPCContextBase() noexcept
{
    if (responder_)
    {
        if (!ClientRPCContextBaseAccess::is_finished(*this))
        {
            client_context_.TryCancel();
        }
        std::default_delete<Responder>{}(responder_);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CLIENT_RPC_CONTEXT_BASE_HPP
