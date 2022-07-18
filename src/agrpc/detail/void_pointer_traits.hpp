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

#ifndef AGRPC_DETAIL_VOID_POINTER_TRAITS_HPP
#define AGRPC_DETAIL_VOID_POINTER_TRAITS_HPP

#include <agrpc/detail/config.hpp>

#include <atomic>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
struct VoidPointerTraits;

template <>
struct VoidPointerTraits<void*>
{
    static void* exchange(void*& old_value, void* new_value) noexcept { return std::exchange(old_value, new_value); }
};

template <>
struct VoidPointerTraits<std::atomic<void*>>
{
    static void* exchange(std::atomic<void*>& old_value, void* new_value) noexcept
    {
        return old_value.exchange(new_value);
    }
};
}
AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_VOID_POINTER_TRAITS_HPP
