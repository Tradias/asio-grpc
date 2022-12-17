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

#ifndef AGRPC_DETAIL_MEMORY_RESOURCE_HPP
#define AGRPC_DETAIL_MEMORY_RESOURCE_HPP

#include <agrpc/detail/config.hpp>

#ifdef AGRPC_STANDALONE_ASIO

#if (ASIO_VERSION >= 102201)
#include <asio/recycling_allocator.hpp>
#else
#include <asio/detail/recycling_allocator.hpp>
#endif

#elif defined(AGRPC_BOOST_ASIO)

#if (BOOST_VERSION >= 107900)
#include <boost/asio/recycling_allocator.hpp>
#else
#include <boost/asio/recycling_allocator.hpp>
#endif

#endif

#include <cstddef>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
namespace pmr
{
template <class>
struct polymorphic_allocator;
}

namespace container
{
template <class, class>
struct uses_allocator;
}

struct GrpcContextLocalMemoryResource
{
    constexpr explicit GrpcContextLocalMemoryResource(int) noexcept {}
};

using GrpcContextLocalAllocator =
#ifdef AGRPC_STANDALONE_ASIO
#if (ASIO_VERSION >= 102201)
    ::asio::
#else
    ::asio::detail::
#endif
#elif defined(AGRPC_BOOST_ASIO)
#if (BOOST_VERSION >= 107900)
    ::boost::asio::
#else
    ::boost::asio::detail::
#endif
#endif
        recycling_allocator<std::byte>;

inline auto create_local_allocator(const detail::GrpcContextLocalMemoryResource&) noexcept
{
    return detail::GrpcContextLocalAllocator{};
}

constexpr auto new_delete_resource() noexcept { return 0; }
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_RESOURCE_HPP
