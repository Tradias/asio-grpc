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

#ifndef AGRPC_AGRPC_RPC_TYPE_HPP
#define AGRPC_AGRPC_RPC_TYPE_HPP

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) The type of an RPC
 *
 * @since 2.1.0
 */
enum class RPCType
{
    /**
     * @brief Client-side unary RPC
     */
    CLIENT_UNARY,

    /**
     * @brief Client-side server-streaming RPC
     */
    CLIENT_SERVER_STREAMING,

    /**
     * @brief Client-side client-streaming RPC
     */
    CLIENT_CLIENT_STREAMING,

    /**
     * @brief Client-side bidirectional-streaming RPC
     */
    CLIENT_BIDIRECTIONAL_STREAMING
};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_RPC_TYPE_HPP
