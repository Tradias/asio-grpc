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

#ifndef AGRPC_AGRPC_REPEATEDLYREQUEST_HPP
#define AGRPC_AGRPC_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"
#include "agrpc/detail/initiate.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/rpcs.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class, class = void>
inline constexpr bool HAS_REQUEST_MEMBER_FUNCTION = false;

template <class T>
inline constexpr bool HAS_REQUEST_MEMBER_FUNCTION<T, std::void_t<decltype(std::declval<T&>()->request())>> = true;
}

template <class ImplementationAllocator>
class RepeatedlyRequestContext
{
  public:
    using executor_type = typename std::allocator_traits<ImplementationAllocator>::value_type::executor_type;

    [[nodiscard]] decltype(auto) args() const noexcept { return impl->args(); }

    [[nodiscard]] decltype(auto) server_context() const noexcept { return impl->server_context(); }

    [[nodiscard]] decltype(auto) request() const noexcept
    {
        static_assert(detail::HAS_REQUEST_MEMBER_FUNCTION<detail::AllocatedPointer<ImplementationAllocator>>,
                      "Client-streaming and bidirectional-streaming requests are made without an initial request by "
                      "the client. The .request() member function is therefore not available.");
        return impl->request();
    }

    [[nodiscard]] decltype(auto) responder() const noexcept { return impl->responder(); }

    [[nodiscard]] executor_type get_executor() const noexcept { return impl->get_executor(); }

  private:
    friend detail::RepeatedlyRequestContextAccess;

    detail::AllocatedPointer<ImplementationAllocator> impl;

    constexpr explicit RepeatedlyRequestContext(detail::AllocatedPointer<ImplementationAllocator>&& impl) noexcept
        : impl(std::move(impl))
    {
    }
};

namespace detail
{
class RepeatedlyRequestFn
{
  private:
    template <class RPC, class Service, class Handler, class CompletionToken>
    static auto impl(RPC rpc, Service& service, Handler handler, CompletionToken token);

  public:
    template <class RPC, class Service, class Request, class Responder, class Handler, class CompletionToken>
    auto operator()(detail::ServerMultiArgRequest<RPC, Request, Responder> rpc, Service& service, Handler handler,
                    CompletionToken token) const;

    template <class RPC, class Service, class Responder, class Handler, class CompletionToken>
    auto operator()(detail::ServerSingleArgRequest<RPC, Responder> rpc, Service& service, Handler handler,
                    CompletionToken token) const;
};
}  // namespace detail

inline constexpr detail::RepeatedlyRequestFn repeatedly_request{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLYREQUEST_HPP

#include "agrpc/detail/repeatedlyRequest.hpp"