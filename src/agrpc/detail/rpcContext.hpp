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

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/rpc.hpp>
#include <grpcpp/completion_queue.h>
#include <grpcpp/server_context.h>

#include <tuple>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class RPCContextBase
{
  public:
    constexpr auto& server_context() noexcept { return context; }

  private:
    grpc::ServerContext context{};
};

template <class Request, class Responder>
class MultiArgRPCContext : public detail::RPCContextBase
{
  public:
    using Signature = void(grpc::ServerContext&, Request&, Responder&);

    MultiArgRPCContext() = default;

    constexpr auto args() noexcept
    {
        return std::forward_as_tuple(this->server_context(), this->request_, this->responder_);
    }

    constexpr auto& request() noexcept { return this->request_; }

    constexpr auto& responder() noexcept { return this->responder_; }

  private:
    Request request_{};
    Responder responder_{&this->server_context()};
};

template <class Responder>
class SingleArgRPCContext : public detail::RPCContextBase
{
  public:
    using Signature = void(grpc::ServerContext&, Responder&);

    SingleArgRPCContext() = default;

    constexpr auto args() noexcept { return std::forward_as_tuple(this->server_context(), this->responder_); }

    constexpr auto& responder() noexcept { return this->responder_; }

  private:
    Responder responder_{&this->server_context()};
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
using RPCContextForRPCT = typename detail::RPCContextForRPC<detail::RemoveCvrefT<RPC>>::Type;

template <class RPC, class Service, class Request, class Responder>
void initiate_request_from_rpc_context(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                                       detail::MultiArgRPCContext<Request, Responder>& rpc_context,
                                       grpc::ServerCompletionQueue* cq, void* tag)
{
    (service.*rpc)(&rpc_context.server_context(), &rpc_context.request(), &rpc_context.responder(), cq, cq, tag);
}

template <class RPC, class Service, class Responder>
void initiate_request_from_rpc_context(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service,
                                       detail::SingleArgRPCContext<Responder>& rpc_context,
                                       grpc::ServerCompletionQueue* cq, void* tag)
{
    (service.*rpc)(&rpc_context.server_context(), &rpc_context.responder(), cq, cq, tag);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RPCCONTEXT_HPP
