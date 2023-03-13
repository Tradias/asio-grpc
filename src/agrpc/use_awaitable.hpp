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

#ifndef AGRPC_AGRPC_USE_AWAITABLE_HPP
#define AGRPC_AGRPC_USE_AWAITABLE_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

AGRPC_NAMESPACE_BEGIN()

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
using GrpcAwaitable
    [[deprecated("Asio-grpc no longer depends on <memory_resource> or Boost.Container. All pmr type alias will "
                 "therefore be removed in v3.")]] =
        asio::awaitable<T, agrpc::BasicGrpcExecutor<agrpc::detail::pmr::polymorphic_allocator<std::byte>>>;

/**
 * @brief `asio::use_awaitable_t` specialized on `agrpc::pmr::GrpcExecutor`
 */
using GrpcUseAwaitable
    [[deprecated("Asio-grpc no longer depends on <memory_resource> or Boost.Container. All pmr type alias will "
                 "therefore be removed in v3.")]] =
        asio::use_awaitable_t<agrpc::BasicGrpcExecutor<agrpc::detail::pmr::polymorphic_allocator<std::byte>>>;

/**
 * @brief `asio::use_awaitable` specialized on `agrpc::pmr::GrpcExecutor`
 */
[[deprecated(
    "Asio-grpc no longer depends on <memory_resource> or Boost.Container. All pmr type alias will therefore be removed "
    "in v3.")]] inline constexpr asio::
    use_awaitable_t<agrpc::BasicGrpcExecutor<agrpc::detail::pmr::polymorphic_allocator<std::byte>>>
        GRPC_USE_AWAITABLE{};
}  // namespace pmr

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_USE_AWAITABLE_HPP
