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

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/grpc_executor.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Server/ClientRPC.read in form of a function object
 */
struct ReadFn
{
    /**
     * @brief Read from a ServerRPC
     *
     * Equivalent to performing `rpc.read(req, token)`.
     *
     * @since 2.7.0
     */
    template <auto RequestRPC, class Traits, class Executor,
              class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    decltype(auto) operator()(agrpc::ServerRPC<RequestRPC, Traits, Executor>& rpc,
                              typename agrpc::ServerRPC<RequestRPC, Traits, Executor>::Request& req,
                              CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{}) const
        noexcept(noexcept(rpc.read(req, static_cast<CompletionToken&&>(token))))
    {
        return rpc.read(req, static_cast<CompletionToken&&>(token));
    }

    /**
     * @brief Read from a Client
     *
     * Equivalent to performing `rpc.read(response, token)`.
     *
     * @since 2.7.0
     */
    template <auto PrepareAsync, class Executor, class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    decltype(auto) operator()(agrpc::ClientRPC<PrepareAsync, Executor>& rpc,
                              typename agrpc::ClientRPC<PrepareAsync, Executor>::Response& response,
                              CompletionToken&& token = detail::DefaultCompletionTokenT<Executor>{}) const
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
