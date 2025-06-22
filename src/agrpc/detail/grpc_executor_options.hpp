// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPC_EXECUTOR_OPTIONS_HPP
#define AGRPC_DETAIL_GRPC_EXECUTOR_OPTIONS_HPP

#include <cstdint>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct GrpcExecutorOptions
{
    static constexpr std::uint32_t BLOCKING_NEVER = 1u << 0u;
    static constexpr std::uint32_t OUTSTANDING_WORK_TRACKED = 1u << 1u;

    static constexpr std::uint32_t DEFAULT = BLOCKING_NEVER;

    GrpcExecutorOptions(const GrpcExecutorOptions&) = delete;
    GrpcExecutorOptions(GrpcExecutorOptions&&) = delete;
    GrpcExecutorOptions& operator=(const GrpcExecutorOptions&) = delete;
    GrpcExecutorOptions& operator=(GrpcExecutorOptions&&) = delete;
};

[[nodiscard]] constexpr bool is_blocking_never(std::uint32_t options) noexcept
{
    return (options & detail::GrpcExecutorOptions::BLOCKING_NEVER) != 0u;
}

[[nodiscard]] constexpr std::uint32_t set_blocking_never(std::uint32_t options, bool value) noexcept
{
    if (value)
    {
        options |= detail::GrpcExecutorOptions::BLOCKING_NEVER;
    }
    else
    {
        options &= ~detail::GrpcExecutorOptions::BLOCKING_NEVER;
    }
    return options;
}

[[nodiscard]] constexpr bool is_outstanding_work_tracked(std::uint32_t options) noexcept
{
    return (options & detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED) != 0u;
}

[[nodiscard]] constexpr std::uint32_t set_outstanding_work_tracked(std::uint32_t options, bool value) noexcept
{
    if (value)
    {
        options |= detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED;
    }
    else
    {
        options &= ~detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED;
    }
    return options;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_EXECUTOR_OPTIONS_HPP
