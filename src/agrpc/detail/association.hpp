// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_ASSOCIATION_HPP
#define AGRPC_DETAIL_ASSOCIATION_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/association_asio.hpp>
#else
#include <agrpc/detail/association_unifex.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
using AssociatedAllocatorT = decltype(exec::get_allocator(std::declval<T>()));

template <class T>
using AssociatedExecutorT = decltype(exec::get_executor(std::declval<T>()));
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ASSOCIATION_HPP
