// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_AGRPC_DEFAULT_SERVER_RPC_TRAITS_HPP
#define AGRPC_AGRPC_DEFAULT_SERVER_RPC_TRAITS_HPP

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Default ServerRPC traits
 *
 * Users that intend to customize ServerRPC traits should publicly inherit from this type to ensure forwards
 * compatibility.
 *
 * Example showing how to enable `ServerRPC::wait_for_done()`:
 *
 * @code{cpp}
 * struct MyTraits : agrpc::DefaultServerRPCTraits
 * {
 *     static constexpr bool NOTIFY_WHEN_DONE = true;
 * };
 *
 * template<auto RequestRPC>
 * using MyServerRPC = agrpc::ServerRPC<RequestRPC, MyTraits>;
 * @endcode
 */
struct DefaultServerRPCTraits
{
    /**
     * @brief Whether the ServerRPC should support `wait_for_done`
     */
    static constexpr bool NOTIFY_WHEN_DONE = false;
};

AGRPC_NAMESPACE_END

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_DEFAULT_SERVER_RPC_TRAITS_HPP
