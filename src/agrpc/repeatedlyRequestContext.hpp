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

#ifndef AGRPC_AGRPC_REPEATEDLYREQUESTCONTEXT_HPP
#define AGRPC_AGRPC_REPEATEDLYREQUESTCONTEXT_HPP

#include "agrpc/detail/allocate.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class, class = void>
inline constexpr bool HAS_REQUEST_MEMBER_FUNCTION = false;

template <class T>
inline constexpr bool HAS_REQUEST_MEMBER_FUNCTION<T, std::void_t<decltype(std::declval<T&>()->request())>> = true;
}

/**
 * @brief Context passed to the request handler of repeatedly_request
 *
 * A move-only type that provides a stable address to the `grpc::ServerContext`, the request (if any) and the responder
 * of one request made by repeatedly_request.
 */
template <class ImplementationAllocator>
class RepeatedlyRequestContext
{
  public:
    /**
     * @brief Tuple of `grpc::ServerContext`, the request (if any) and the responder
     *
     * Useful in combination with `std::apply` when implementing generic request handler.
     *
     * The return type depends on the RPC.
     *
     * unary: `std::tuple<grpc::ServerContext&, Request&, grpc::ServerAsyncResponseWriter<Response>&>`<br>
     * server-streaming: `std::tuple<grpc::ServerContext&, Request&, grpc::ServerAsyncWriter<Response>&>`<br>
     * client-streaming: `std::tuple<grpc::ServerContext&, grpc::ServerAsyncReader<Response, Request>&>`<br>
     * bidirectional-streaming: `std::tuple<grpc::ServerContext&, grpc::ServerAsyncReaderWriter<Response, Request>&>`
     */
    [[nodiscard]] decltype(auto) args() const noexcept { return impl->args(); }

    /**
     * @brief Reference to the `grpc::ServerContext` of this request
     */
    [[nodiscard]] decltype(auto) server_context() const noexcept { return impl->server_context(); }

    /**
     * @brief Reference to the request
     *
     * Only available for unary and server-streaming RPCs. Other RPCs are made without an initial request by the client.
     */
    [[nodiscard]] decltype(auto) request() const noexcept
    {
        static_assert(detail::HAS_REQUEST_MEMBER_FUNCTION<detail::AllocatedPointer<ImplementationAllocator>>,
                      "Client-streaming and bidirectional-streaming requests are made without an initial request by "
                      "the client. The .request() member function is therefore not available.");
        return impl->request();
    }

    /**
     * @brief Reference to the responder
     *
     * The return type depends on the RPC.
     *
     * unary: `grpc::ServerAsyncResponseWriter<Response>&`<br>
     * server-streaming: `grpc::ServerAsyncWriter<Response>&`<br>
     * client-streaming: `grpc::ServerAsyncReader<Response, Request>&`<br>
     * bidirectional-streaming: `grpc::ServerAsyncReaderWriter<Response, Request>&`
     */
    [[nodiscard]] decltype(auto) responder() const noexcept { return impl->responder(); }

  private:
    friend detail::RepeatedlyRequestContextAccess;

    detail::AllocatedPointer<ImplementationAllocator> impl;

    explicit RepeatedlyRequestContext(detail::AllocatedPointer<ImplementationAllocator>&& impl) noexcept
        : impl(std::move(impl))
    {
    }
};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLYREQUESTCONTEXT_HPP
