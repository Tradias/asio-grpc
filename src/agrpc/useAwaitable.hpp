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

#ifndef AGRPC_AGRPC_USEAWAITABLE_HPP
#define AGRPC_AGRPC_USEAWAITABLE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

AGRPC_NAMESPACE_BEGIN()

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
/**
 * @brief `asio::awaitable` specialized on `agrpc::GrpcExecutor`
 */
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable_t` specialized on `agrpc::GrpcExecutor`
 */
using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable` specialized on `agrpc::GrpcExecutor`
 */
inline constexpr agrpc::GrpcUseAwaitable GRPC_USE_AWAITABLE{};

namespace pmr
{
/**
 * @brief `asio::awaitable` specialized on `agrpc::pmr::GrpcExecutor`
 */
template <class T>
using GrpcAwaitable = asio::awaitable<T, agrpc::pmr::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable_t` specialized on `agrpc::pmr::GrpcExecutor`
 */
using GrpcUseAwaitable = asio::use_awaitable_t<agrpc::pmr::GrpcExecutor>;

/**
 * @brief `asio::use_awaitable` specialized on `agrpc::pmr::GrpcExecutor`
 */
inline constexpr agrpc::pmr::GrpcUseAwaitable GRPC_USE_AWAITABLE{};
}  // namespace pmr
#endif

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_USEAWAITABLE_HPP
