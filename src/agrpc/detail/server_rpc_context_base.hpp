// Copyright 2026 Dennis Hezel
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

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/rpc_executor_base.hpp>
#include <agrpc/detail/server_rpc_notify_when_done_base.hpp>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/server_context.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Responder>
struct ServerContextForResponder
{
    using Type = grpc::ServerContext;
};

template <>
struct ServerContextForResponder<grpc::GenericServerAsyncReaderWriter>
{
    using Type = grpc::GenericServerContext;
};

/**
 * @brief ServerRPC ServerContext base
 *
 * @since 2.6.0
 */
template <class Responder>
class ServerRPCContextBase
{
  public:
    /**
     * @brief Get the underlying `ServerContext`
     */
    [[nodiscard]] auto& context() { return server_context_; }

    /**
     * @brief Get the underlying `ServerContext` (const overload)
     */
    [[nodiscard]] const auto& context() const { return server_context_; }

    /**
     * @brief Cancel this RPC
     *
     * Effectively calls `context().TryCancel()`.
     *
     * Thread-safe
     */
    void cancel() noexcept { server_context_.TryCancel(); }

  private:
    using ServerContext = typename ServerContextForResponder<Responder>::Type;

    friend detail::ServerRPCContextBaseAccess;

    template <bool, class, class>
    friend class detail::ServerRPCNotifyWhenDoneMixin;

    ServerContext server_context_;
    Responder responder_{&server_context_};
    bool is_finished_{};
};

template <class Responder, bool IsNotifyWhenDone>
class ServerRPCResponderAndNotifyWhenDone : public ServerRPCContextBase<Responder>,
                                            public ServerRPCNotifyWhenDoneBase<IsNotifyWhenDone>
{
  protected:
    using ServerRPCContextBase<Responder>::ServerRPCContextBase;

  private:
    friend detail::ServerRPCContextBaseAccess;
};

struct ServerRPCContextBaseAccess
{
    template <class ServerRPC>
    static auto construct(const typename ServerRPC::executor_type& executor)
    {
        return ServerRPC(executor);
    }

    template <class ServerRPC>
    static constexpr auto service(ServerRPC&) -> typename ServerRPC::Service;

    template <class Responder>
    static Responder& responder(ServerRPCContextBase<Responder>& rpc) noexcept
    {
        return rpc.responder_;
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
    static void initiate_notify_when_done(ServerRPCResponderAndNotifyWhenDone<Responder, IsNotifyWhenDone>& rpc)
    {
        if constexpr (IsNotifyWhenDone)
        {
            rpc.initiate_notify_when_done(rpc.server_context_);
        }
    }
};

template <class ServerRPC>
using ServerRPCServiceT = decltype(detail::ServerRPCContextBaseAccess::service(std::declval<ServerRPC&>()));
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_RPC_CONTEXT_BASE_HPP
