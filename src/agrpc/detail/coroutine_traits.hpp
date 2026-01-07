// Copyright 2026 Dennis Hezel
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

#include <agrpc/detail/awaitable.hpp>

#ifdef AGRPC_ASIO_HAS_CO_AWAIT

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/co_spawn.hpp>
#include <agrpc/detail/rethrow_first_arg.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Coroutine>
struct CoroutineTraits;

template <class T, class Executor>
struct CoroutineTraits<asio::awaitable<T, Executor>>
{
    using ReturnType = asio::awaitable<void, Executor>;

    template <class RPCHandler, class CompletionHandler>
    static asio::use_awaitable_t<Executor> completion_token(RPCHandler&, CompletionHandler&)
    {
        return {};
    }

    template <class RPCHandler, class CompletionHandler, class IoExecutor, class Function>
    static void co_spawn(const IoExecutor& io_executor, RPCHandler&, CompletionHandler& completion_handler,
                         Function&& function)
    {
        asio::co_spawn(assoc::get_associated_executor(completion_handler, io_executor),
                       static_cast<Function&&>(function), detail::RethrowFirstArg{});
    }
};
}

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_DETAIL_COROUTINE_TRAITS_HPP
