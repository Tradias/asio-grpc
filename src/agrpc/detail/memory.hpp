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

#ifndef AGRPC_DETAIL_MEMORY_HPP
#define AGRPC_DETAIL_MEMORY_HPP

#include <agrpc/detail/config.hpp>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
struct UnwrapUniquePtr
{
    using Type = T;
};

template <class T>
struct UnwrapUniquePtr<std::unique_ptr<T>>
{
    using Type = T;
};

template <class T>
struct UnwrapUniquePtr<const std::unique_ptr<T>>
{
    using Type = T;
};

template <class T>
using UnwrapUniquePtrT = typename detail::UnwrapUniquePtr<T>::Type;

template <class T>
auto& unwrap_unique_ptr(T& t) noexcept
{
    return t;
}

template <class T>
auto& unwrap_unique_ptr(std::unique_ptr<T>& t) noexcept
{
    return *t;
}

template <class T>
auto& unwrap_unique_ptr(const std::unique_ptr<T>& t) noexcept
{
    return *t;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MEMORY_HPP
