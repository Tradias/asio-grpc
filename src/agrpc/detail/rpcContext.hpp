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

#ifndef AGRPC_DETAIL_RPCCONTEXT_HPP
#define AGRPC_DETAIL_RPCCONTEXT_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/rpcs.hpp"

#include <grpcpp/server_context.h>

#include <functional>
#include <tuple>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RPCContextBase
{
    grpc::ServerContext context{};

    constexpr auto& server_context() noexcept { return context; }
};

template <class Request, class Responder>
struct MultiArgRPCContext : detail::RPCContextBase
{
    Responder responder_{&this->context};
    Request request_{};

    MultiArgRPCContext() = default;

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->request_, this->responder_,
                           std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->request_, this->responder_); }

    constexpr auto& request() noexcept { return this->request_; }

    constexpr auto& responder() noexcept { return this->responder_; }
};

template <class Responder>
struct SingleArgRPCContext : detail::RPCContextBase
{
    Responder responder_{&this->context};

    SingleArgRPCContext() = default;

    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args)
    {
        return std::invoke(std::forward<Handler>(handler), this->context, this->responder_,
                           std::forward<Args>(args)...);
    }

    constexpr auto args() noexcept { return std::forward_as_tuple(this->context, this->responder_); }

    constexpr auto& responder() noexcept { return this->responder_; }
};

template <class>
struct RPCContextForRPC;

template <class RPC, class Request, class Responder>
struct RPCContextForRPC<detail::ServerMultiArgRequest<RPC, Request, Responder>>
{
    using Type = detail::MultiArgRPCContext<Request, Responder>;
};

template <class RPC, class Responder>
struct RPCContextForRPC<detail::ServerSingleArgRequest<RPC, Responder>>
{
    using Type = detail::SingleArgRPCContext<Responder>;
};

template <class RPC>
using RPCContextForRPCT = typename detail::RPCContextForRPC<RPC>::Type;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPCCONTEXT_HPP
