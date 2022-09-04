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

#ifndef AGRPC_DETAIL_COROUTINE_TRAITS_HPP
#define AGRPC_DETAIL_COROUTINE_TRAITS_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>

#include <exception>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct RethrowFirstArg
{
    template <class... Args>
    void operator()(std::exception_ptr ep, Args&&...) const
    {
        if AGRPC_UNLIKELY (ep)
        {
            std::rethrow_exception(ep);
        }
    }
};

struct CompletionHandlerUnknown
{
    char d[256];
};

template <class CompletionToken, class Signature, class = void>
struct HandlerType
{
    using Type = CompletionHandlerUnknown;
};

template <class CompletionToken, class Signature>
struct HandlerType<CompletionToken, Signature,
                   std::void_t<typename asio::async_result<CompletionToken, Signature>::handler_type>>
{
    using Type = typename asio::async_result<CompletionToken, Signature>::handler_type;
};

template <class CompletionToken, class Signature, class = void>
struct CompletionHandlerType
{
    using Type = typename detail::HandlerType<CompletionToken, Signature>::Type;
};

template <class CompletionToken, class Signature>
struct CompletionHandlerType<
    CompletionToken, Signature,
    std::void_t<typename asio::async_result<CompletionToken, Signature>::completion_handler_type>>
{
    using Type = typename asio::async_result<CompletionToken, Signature>::completion_handler_type;
};

template <class CompletionToken, class Signature>
using CompletionHandlerTypeT = typename CompletionHandlerType<CompletionToken, Signature>::Type;

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

template <class Coroutine>
using CoroutineExecutorT = typename detail::CoroutineTraits<Coroutine>::ExecutorType;
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_COROUTINE_TRAITS_HPP
