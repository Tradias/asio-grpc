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

#ifndef AGRPC_AGRPC_REPEATEDLYREQUEST_HPP
#define AGRPC_AGRPC_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/rpcs.hpp"

AGRPC_NAMESPACE_BEGIN()

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class RPCContextImplementationAllocator>
class RPCRequestContext
{
  public:
    template <class Handler, class... Args>
    constexpr decltype(auto) operator()(Handler&& handler, Args&&... args) const
    {
        return (*impl)(std::forward<Handler>(handler), std::forward<Args>(args)...);
    }

    constexpr auto args() const noexcept { return impl->args(); }

  private:
    friend detail::RPCContextImplementation;

    detail::AllocatedPointer<RPCContextImplementationAllocator> impl;

    constexpr explicit RPCRequestContext(detail::AllocatedPointer<RPCContextImplementationAllocator>&& impl) noexcept
        : impl(std::move(impl))
    {
    }
};

namespace detail
{
struct RepeatedlyRequestFn
{
    template <class RPC, class Service, class Request, class Responder, class Handler>
    void operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service,
                    Handler handler) const;

    template <class RPC, class Service, class Responder, class Handler>
    void operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, Handler handler) const;
};
}  // namespace detail

inline constexpr detail::RepeatedlyRequestFn repeatedly_request{};
#endif

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/repeatedlyRequest.hpp"