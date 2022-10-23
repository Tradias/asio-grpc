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

#ifndef AGRPC_AGRPC_ASYNC_NOTIFY_WHEN_DONE_HPP
#define AGRPC_AGRPC_ASYNC_NOTIFY_WHEN_DONE_HPP

#include <agrpc/default_completion_token.hpp>
#include <agrpc/detail/async_notify_when_done.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/initiate_sender_implementation.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Server-side function to set notification for RPC completion
 *
 * The examples below are based on the following .proto file:
 *
 * @snippet example.proto example-proto
 *
 * **Per-Operation Cancellation**
 *
 * None.
 */
struct AsyncNotfiyWhenDoneFn
{
    /**
     * @brief Read initial metadata
     *
     * Request notification of the reading of the initial metadata.
     *
     * This call is optional, but if it is used, it cannot be used concurrently with or after the read method.
     *
     * Side effect:
     *
     * @arg Upon receiving initial metadata from the server, the ClientContext associated with this call is updated, and
     * the calling code can access the received metadata through the ClientContext.
     *
     * Example:
     *
     * @snippet client.cpp read_initial_metadata-unary-client-side
     *
     * @attention For client-streaming and bidirectional-streaming RPCs: If the server does not explicitly send initial
     * metadata (e.g. by calling `agrpc::send_initial_metadata`) but waits for a message from the client instead then
     * this function won't complete until `agrpc::write` is called.
     *
     * @param responder `grpc::ClientAsyncResponseReader`, `grpc::ClientAsyncReader`, `grpc::ClientAsyncWriter` or
     * `grpc::ClientAsyncReaderWriter` (or a unique_ptr of them or their -Interface variants).
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`. `true` indicates that the metadata was read, `false` when the call is
     * dead.
     */
    template <class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(agrpc::GrpcContext& grpc_context, grpc::ServerContext& server_context,
                    CompletionToken&& token = {}) const
        noexcept(std::is_same_v<agrpc::UseSender, detail::RemoveCrefT<CompletionToken>>)
    {
        return detail::async_initiate_sender_implementation<detail::AsyncNotfiyWhenDoneSenderImplementation>(
            grpc_context, {}, {grpc_context, server_context}, token);
    }
};
}  // namespace detail

/**
 * @brief Set notification for server-side RPC completion
 *
 * @link detail::AsyncNotfiyWhenDoneFn
 * Server-side function to set notification for RPC completion.
 * @endlink
 */
inline constexpr detail::AsyncNotfiyWhenDoneFn async_notify_when_done{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_ASYNC_NOTIFY_WHEN_DONE_HPP
