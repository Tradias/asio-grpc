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

#ifndef AGRPC_DETAIL_COROUTINE_TRAITS_HPP
#define AGRPC_DETAIL_COROUTINE_TRAITS_HPP

#include <agrpc/detail/asio_forward.hpp>

#include <agrpc/detail/awaitable.hpp>
#include <agrpc/detail/config.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Coroutine>
struct CoroutineTraits;

template <class T, class Executor>
struct CoroutineTraits<asio::awaitable<T, Executor>>
{
    using ExecutorType = Executor;
    using CompletionToken = asio::use_awaitable_t<Executor>;

    template <class U>
    using Rebind = asio::awaitable<U, Executor>;
};

template <class Coroutine, class ReturnType>
using RebindCoroutineT = typename detail::CoroutineTraits<Coroutine>::template Rebind<ReturnType>;

template <class Coroutine>
using CoroutineCompletionTokenT = typename detail::CoroutineTraits<Coroutine>::CompletionToken;
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_COROUTINE_TRAITS_HPP
