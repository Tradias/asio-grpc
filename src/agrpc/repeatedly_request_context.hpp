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

#ifndef AGRPC_AGRPC_REPEATEDLY_REQUEST_CONTEXT_HPP
#define AGRPC_AGRPC_REPEATEDLY_REQUEST_CONTEXT_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#ifdef AGRPC_HAS_CONCEPTS
template <class T>
concept HAS_REQUEST_MEMBER_FUNCTION = requires(T& t) { t->request(); };
#else
template <class, class = void>
inline constexpr bool HAS_REQUEST_MEMBER_FUNCTION = false;

template <class T>
inline constexpr bool HAS_REQUEST_MEMBER_FUNCTION<T, decltype((void)std::declval<T&>()->request())> = true;
#endif
}

/**
 * @brief Context passed to the request handler of repeatedly_request
 *
 * A move-only type that provides a stable address to the `grpc::ServerContext`, the request (if any) and the responder
 * of one request made by repeatedly_request.
 */
template <class Allocator>
class RepeatedlyRequestContext
{
  public:
    /**
     * @brief Tuple of `grpc::ServerContext`, the request (if any) and the responder
     *
     * Useful in combination with `std::apply` when implementing request handler templates.
     *
     * The return type depends on the RPC.
     *
     * unary: `std::tuple<grpc::ServerContext&, Request&, grpc::ServerAsyncResponseWriter<Response>&>`<br>
     * server-streaming: `std::tuple<grpc::ServerContext&, Request&, grpc::ServerAsyncWriter<Response>&>`<br>
     * client-streaming: `std::tuple<grpc::ServerContext&, grpc::ServerAsyncReader<Response, Request>&>`<br>
     * bidirectional-streaming: `std::tuple<grpc::ServerContext&, grpc::ServerAsyncReaderWriter<Response, Request>&>`
     */
    [[nodiscard]] decltype(auto) args() const noexcept { return impl_->args(); }

    /**
     * @brief Reference to the `grpc::ServerContext` of this request
     */
    [[nodiscard]] decltype(auto) server_context() const noexcept { return impl_->server_context(); }

    /**
     * @brief Reference to the request
     *
     * Only available for unary and server-streaming RPCs. Other RPCs are made without an initial request by the client.
     */
    [[nodiscard]] decltype(auto) request() const noexcept
    {
        static_assert(
            detail::HAS_REQUEST_MEMBER_FUNCTION<detail::AllocatedPointer<Allocator>>,
            "Client-streaming, bidirectional-streaming and generic requests are made without an initial request by "
            "the client. The .request() member function is therefore not available.");
        return impl_->request();
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
    [[nodiscard]] decltype(auto) responder() const noexcept { return impl_->responder(); }

  private:
    using Impl = detail::AllocatedPointer<Allocator>;

    friend detail::RepeatedlyRequestContextAccess;

    explicit RepeatedlyRequestContext(Impl&& impl) noexcept : impl_(static_cast<Impl&&>(impl)) {}

    Impl impl_;
};

/**
 * @brief The RepeatedlyRequestContext for generic RPC requests
 */
template <class Allocator = std::allocator<void>>
using GenericRepeatedlyRequestContext = agrpc::RepeatedlyRequestContext<
    typename std::allocator_traits<Allocator>::template rebind_alloc<detail::GenericRPCContext>>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_REPEATEDLY_REQUEST_CONTEXT_HPP
