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

#ifndef AGRPC_DETAIL_SERVER_RPC_CONTEXT_BASE_HPP
#define AGRPC_DETAIL_SERVER_RPC_CONTEXT_BASE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/server_rpc_notify_when_done_base.hpp>
#include <grpcpp/server_context.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief (experimental) ServerRPC grpc::ServerContext base
 *
 * @since 2.6.0
 */
template <class Responder>
class ServerRPCContextBase
{
  public:
    /**
     * @brief Get the underlying `grpc::ServerContext`
     */
    [[nodiscard]] grpc::ServerContext& context() { return server_context_; }

    /**
     * @brief Get the underlying `grpc::ServerContext` (const overload)
     */
    [[nodiscard]] const grpc::ServerContext& context() const { return server_context_; }

    /**
     * @brief Cancel this RPC
     *
     * Effectively calls `context().TryCancel()`.
     *
     * Thread-safe
     */
    void cancel() noexcept { server_context_.TryCancel(); }

    [[nodiscard]] bool is_finished() const noexcept { return is_finished_; }

  protected:
    ServerRPCContextBase() = default;

    template <class ServerContextInitFunction>
    explicit ServerRPCContextBase(ServerContextInitFunction&& init_function) noexcept
    {
        static_cast<ServerContextInitFunction&&>(init_function)(server_context_);
    }

    ServerRPCContextBase(const ServerRPCContextBase&) = delete;

    ServerRPCContextBase(ServerRPCContextBase&& other) = delete;

    ~ServerRPCContextBase() noexcept
    {
        if (is_started_ && !is_finished_)
        {
            server_context_.TryCancel();
        }
    }

    ServerRPCContextBase& operator=(const ServerRPCContextBase&) = delete;

    ServerRPCContextBase& operator=(ServerRPCContextBase&& other) = delete;

  private:
    friend detail::ServerRPCContextBaseAccess;

    grpc::ServerContext server_context_{};
    Responder responder_{&server_context_};
    bool is_started_{};
    bool is_finished_{};
};

template <class Responder, bool IsNotifyWhenDone>
class ServerRPCBase : public ServerRPCContextBase<Responder>, public ServerRPCNotifyWhenDoneBase<IsNotifyWhenDone>
{
  private:
    friend detail::ServerRPCContextBaseAccess;
};

struct ServerRPCContextBaseAccess
{
    template <class Responder>
    static Responder& responder(ServerRPCContextBase<Responder>& rpc) noexcept
    {
        return rpc.responder_;
    }

    template <class Responder>
    static void set_started(ServerRPCContextBase<Responder>& rpc) noexcept
    {
        rpc.is_started_ = true;
    }

    template <class Responder>
    [[nodiscard]] static bool is_finished(ServerRPCContextBase<Responder>& rpc) noexcept
    {
        return rpc.is_finished_;
    }

    template <class Responder>
    static void set_finished(ServerRPCContextBase<Responder>& rpc) noexcept
    {
        rpc.is_finished_ = true;
    }

    template <class Responder, bool IsNotifyWhenDone>
    static void initiate_notify_when_done(ServerRPCBase<Responder, IsNotifyWhenDone>& rpc,
                                          agrpc::GrpcContext& grpc_context)
    {
        rpc.initiate(grpc_context, rpc.server_context_);
    }

    // template <class Responder>
    // [[nodiscard]] static bool is_writes_done(ServerRPCContextBase<Responder>& rpc) noexcept
    // {
    //     return rpc.responder_.template has_bit<1>();
    // }

    // template <class Responder>
    // static void set_writes_done(ServerRPCContextBase<Responder>& rpc) noexcept
    // {
    //     rpc.responder_.template set_bit<1>();
    // }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_CONTEXT_BASE_HPP
