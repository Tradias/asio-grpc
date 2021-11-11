// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPCCONTEXT_HPP
#define AGRPC_DETAIL_GRPCCONTEXT_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/memoryResource.hpp"

#include <cstddef>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using GrpcContextLocalMemoryResource = detail::pmr::unsynchronized_pool_resource;
using GrpcContextLocalAllocator = detail::MemoryResourceAllocator<std::byte, detail::GrpcContextLocalMemoryResource>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXT_HPP
