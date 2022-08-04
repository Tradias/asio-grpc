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

#ifndef AGRPC_HELPER_BUFFER_HPP
#define AGRPC_HELPER_BUFFER_HPP

#include <agrpc/bind_allocator.hpp>
#include <agrpc/detail/one_shot_allocator.hpp>

#include <cstddef>
#include <type_traits>

namespace example
{
template <std::size_t Capacity>
struct Buffer
{
    // You should copy the implementation of OneShotAllocator into your code if you intent to use it. Do not worry,
    // it is very simple!
    using allocator_type = agrpc::detail::OneShotAllocator<std::byte, Capacity>;

    [[nodiscard]] allocator_type allocator() noexcept { return allocator_type{buffer}; }

    template <class Target>
    [[nodiscard]] auto bind_allocator(Target&& target) noexcept
    {
        return agrpc::bind_allocator(this->allocator(), std::forward<Target>(target));
    }

    alignas(std::max_align_t) std::byte buffer[Capacity];
};
}

#endif  // AGRPC_HELPER_BUFFER_HPP
