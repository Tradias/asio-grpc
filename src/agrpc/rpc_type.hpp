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

#ifndef AGRPC_AGRPC_RPC_TYPE_HPP
#define AGRPC_AGRPC_RPC_TYPE_HPP

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief The type of a ClientRPC
 *
 * @since 2.1.0
 */
enum class ClientRPCType
{
    /**
     * @brief Client-side unary rpc
     */
    UNARY,

    /**
     * @brief Client-side generic unary rpc
     */
    GENERIC_UNARY,

    /**
     * @brief Client-side server-streaming rpc
     */
    SERVER_STREAMING,

    /**
     * @brief Client-side client-streaming rpc
     */
    CLIENT_STREAMING,

    /**
     * @brief Client-side bidirectional-streaming rpc
     */
    BIDIRECTIONAL_STREAMING,

    /**
     * @brief Client-side generic streaming rpc
     */
    GENERIC_STREAMING
};

/**
 * @brief The type of a ServerRPC
 *
 * @since 2.7.0
 */
enum class ServerRPCType
{
    /**
     * @brief Server-side unary rpc
     */
    UNARY,

    /**
     * @brief Server-side server-streaming rpc
     */
    SERVER_STREAMING,

    /**
     * @brief Server-side client-streaming rpc
     */
    CLIENT_STREAMING,

    /**
     * @brief Server-side bidirectional-streaming rpc
     */
    BIDIRECTIONAL_STREAMING,

    /**
     * @brief Server-side generic streaming rpc
     */
    GENERIC
};

AGRPC_NAMESPACE_END

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_RPC_TYPE_HPP
