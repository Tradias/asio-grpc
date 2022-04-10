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

#ifndef AGRPC_UTILS_ALLOCATOR_HPP
#define AGRPC_UTILS_ALLOCATOR_HPP

#include "utils/asioForward.hpp"

#include <agrpc/grpcContext.hpp>
#include <agrpc/repeatedlyRequestContext.hpp>

#include <type_traits>

namespace test
{
template <class T, class Allocator, class... Args>
auto allocate(Allocator allocator, Args&&... args)
{
    using PmrAllocator = agrpc::detail::pmr::polymorphic_allocator<std::byte>;
    using Traits = typename std::allocator_traits<Allocator>::template rebind_traits<T>;
    typename Traits::allocator_type rebound_allocator{allocator};
    auto* ptr = Traits::allocate(rebound_allocator, 1);
    Traits::construct(rebound_allocator, ptr, std::forward<Args>(args)...);
    auto deleter = [=](T* p) mutable
    {
        Traits::destroy(rebound_allocator, p);
        Traits::deallocate(rebound_allocator, p, 1);
    };
    return std::unique_ptr<T, decltype(deleter)>{ptr, deleter};
}
}  // namespace test

#endif  // AGRPC_UTILS_ALLOCATOR_HPP
