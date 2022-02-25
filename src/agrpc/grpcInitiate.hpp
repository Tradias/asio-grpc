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

#ifndef AGRPC_AGRPC_GRPCINITIATE_HPP
#define AGRPC_AGRPC_GRPCINITIATE_HPP

#include "agrpc/defaultCompletionToken.hpp"
#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcInitiate.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
/**
 * @brief Function object to initiate gRPC tag-based functions
 */
struct GrpcInitiateFn
{
    /**
     * @brief Initiate a gRPC tag-based function
     *
     * This function can be used to lift tag-based gRPC functions that are not covered by `rpc.hpp` into the
     * Asio/unifex world.
     *
     * Example:
     *
     * @snippet client.cpp grpc_initiate-NotifyOnStateChange
     *
     * @param initiating_function Signature must be `void(agrpc::GrpcContext&, void*)`.
     * @param token A completion token like `asio::yield_context` or the one created by `agrpc::use_sender`. The
     * completion signature is `void(bool)`.
     */
    template <class InitiatingFunction, class CompletionToken = agrpc::DefaultCompletionToken>
    auto operator()(InitiatingFunction initiating_function, CompletionToken&& token = {}) const
    {
        return detail::grpc_initiate(std::move(initiating_function), std::forward<CompletionToken>(token));
    }
};
}  // namespace detail

/**
 * @brief Initiate a gRPC tag-based function
 *
 * @link detail::GrpcInitiateFn
 * Function to initiate gRPC tag-based functions.
 * @endlink
 */
inline constexpr detail::GrpcInitiateFn grpc_initiate{};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_GRPCINITIATE_HPP
