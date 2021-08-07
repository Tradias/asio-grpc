#ifndef AGRPC_DETAIL_GRPCEXECUTOROPTIONS_HPP
#define AGRPC_DETAIL_GRPCEXECUTOROPTIONS_HPP

#include <cstdint>

namespace agrpc::detail
{
struct GrpcExecutorOptions
{
    static constexpr std::uint32_t BLOCKING_NEVER = 1u << 0u;
    static constexpr std::uint32_t RELATIONSHIP_CONTINUATION = 1u << 1u;
    static constexpr std::uint32_t OUTSTANDING_WORK_TRACKED = 1u << 2u;

    static constexpr std::uint32_t DEFAULT = BLOCKING_NEVER;
};

constexpr bool is_blocking_never(std::uint32_t options) noexcept
{
    return (options & detail::GrpcExecutorOptions::BLOCKING_NEVER) != 0u;
}

constexpr std::uint32_t set_blocking_never(std::uint32_t options, bool value) noexcept
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

constexpr bool is_relationship_continuation(std::uint32_t options) noexcept
{
    return (options & detail::GrpcExecutorOptions::RELATIONSHIP_CONTINUATION) != 0u;
}

constexpr std::uint32_t set_relationship_continuation(std::uint32_t options, bool value) noexcept
{
    if (value)
    {
        options |= detail::GrpcExecutorOptions::RELATIONSHIP_CONTINUATION;
    }
    else
    {
        options &= ~detail::GrpcExecutorOptions::RELATIONSHIP_CONTINUATION;
    }
    return options;
}

constexpr bool is_outstanding_work_tracked(std::uint32_t options) noexcept
{
    return (options & detail::GrpcExecutorOptions::OUTSTANDING_WORK_TRACKED) != 0u;
}

constexpr std::uint32_t set_outstanding_work_tracked(std::uint32_t options, bool value) noexcept
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
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCEXECUTOROPTIONS_HPP
