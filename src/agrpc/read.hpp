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

#ifndef AGRPC_AGRPC_READ_HPP
#define AGRPC_AGRPC_READ_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_initiate.hpp>
#include <agrpc/detail/memory.hpp>
#include <agrpc/detail/rpc.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Client and server-side function object to read from streaming RPCs
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * @attention The completion handler created from the completion token that is provided to the functions described below
 * must have an associated executor that refers to a GrpcContext:
 * @snippet server.cpp bind-executor-to-use-awaitable
 *
 * **Per-Operation Cancellation**
 *
 * None. Operations will be cancelled when the deadline of the RPC has been reached
 * (see
 * [grpc::ClientContext::set_deadline](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#ad4e16866fee3f6ee5a10efb5be6f4da6))
 * or the call has been cancelled
 * (see
 * [grpc::ClientContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_client_context.html#abd0f6715c30287b75288015eee628984)
 * and
 * [grpc::ServerContext::TryCancel](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_context.html#a88d3a0c3d53e39f38654ce8fba968301)).
 */
struct ReadFn
{
    /**
     * @brief Read from a streaming RPC
     *
     * This is thread-safe with respect to write or writes_done methods. It should not be called concurrently with other
     * streaming APIs on the same stream. It is not meaningful to call it concurrently with another read on the same
     * stream since reads on the same stream are delivered in order (expect for server-side bidirectional streams where
     * the order is undefined).
     *
     * Example server-side client-streaming:
     *
     * @snippet server.cpp read-client-streaming-server-side
     *
     * Example server-side bidirectional-streaming:
     *
     * @snippet server.cpp read-bidirectional-streaming-server-side
     *
     * Example client-side server-streaming:
     *
     * @snippet client.cpp read-server-streaming-client-side
     *
     * Example client-side bidirectional-streaming:
     *
     * @snippet client.cpp read-bidirectional-client-side
     *
     * @param reader A `grpc::Client/ServerAsyncReader(Writer)(Interface)` or a `std::unique_ptr` of it.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that a valid message was read. `false` when
     * there will be no more incoming messages, either because the other side has called WritesDone() or the stream has
     * failed (or been cancelled).
     */
    template <class Reader, class Response, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(Reader& reader, Response& response, CompletionToken&& token = {}) const
        noexcept(detail::IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN<CompletionToken>)
    {
        return detail::grpc_initiate(
            detail::ReadInitFunction<Response, detail::UnwrapUniquePtrT<Reader>>{detail::unwrap_unique_ptr(reader),
                                                                                 response},
            static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief (experimental) Read from a ServerRPC
     *
     * Equivalent to performing `rpc.read(req, token)`.
     *
     * @since 2.7.0
     */
    template <auto RequestRPC, class Traits, class Executor,
              class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    decltype(auto) operator()(agrpc::ServerRPC<RequestRPC, Traits, Executor>& rpc,
                              typename agrpc::ServerRPC<RequestRPC, Traits, Executor>::Request & req,
                              CompletionToken && token = detail::DefaultCompletionTokenT<Executor>{}) const
        noexcept(noexcept(rpc.read(req, static_cast<CompletionToken&&>(token))))
    {
        return rpc.read(req, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief (experimental) Read from a Client
     *
     * Equivalent to performing `rpc.read(response, token)`.
     *
     * @since 2.7.0
     */
    template <auto PrepareAsync, class Executor, class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    decltype(auto) operator()(agrpc::ClientRPC<PrepareAsync, Executor>& rpc,
                              typename agrpc::ClientRPC<PrepareAsync, Executor>::Response & response,
                              CompletionToken && token = detail::DefaultCompletionTokenT<Executor>{}) const
        noexcept(noexcept(rpc.read(response, static_cast<CompletionToken&&>(token))))
    {
        return rpc.read(response, static_cast<CompletionToken&&>(token));
    }
};
}  // namespace detail

/**
 * @brief Read from a streaming RPC
 *
 * @link detail::ReadFn
 * Client and server-side function to read from streaming RPCs.
 * @endlink
 */
inline constexpr detail::ReadFn read{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_READ_HPP
