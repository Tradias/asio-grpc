// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_PREPEND_ERROR_CODE_HPP
#define AGRPC_DETAIL_PREPEND_ERROR_CODE_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/tuple.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
struct PrependErrorCodeToSignature;

template <class... Args>
struct PrependErrorCodeToSignature<void(detail::ErrorCode, Args...)>
{
    using Type = void(detail::ErrorCode, Args...);

    template <class Function>
    static void invoke_with_default_args(Function&& function, detail::ErrorCode&& ec)
    {
        static_cast<Function&&>(function)(static_cast<detail::ErrorCode&&>(ec), Args{}...);
    }
};

template <class... Args>
struct PrependErrorCodeToSignature<void(Args...)>
{
    using Type = void(detail::ErrorCode, Args...);

    template <class Function>
    static void invoke_with_default_args(Function&& function, detail::ErrorCode&& ec)
    {
        static_cast<Function&&>(function)(static_cast<detail::ErrorCode&&>(ec), Args{}...);
    }
};

template <class Signature>
using PrependErrorCodeToSignatureT = typename detail::PrependErrorCodeToSignature<Signature>::Type;

template <class CompletionHandler, class... Args, std::size_t... I>
void prepend_error_code_and_apply_impl(CompletionHandler&& ch, detail::Tuple<Args...>&& args,
                                       const std::index_sequence<I...>&)
{
    static_cast<CompletionHandler&&>(ch)(detail::ErrorCode{},
                                         detail::get<I>(static_cast<detail::Tuple<Args...>&&>(args))...);
}

template <class CompletionHandler, class... Args, std::size_t... I>
void prepend_error_code_and_apply_impl(CompletionHandler&& ch, detail::Tuple<detail::ErrorCode, Args...>&& args,
                                       const std::index_sequence<I...>&)
{
    static_cast<CompletionHandler&&>(ch)(
        detail::get<I>(static_cast<detail::Tuple<detail::ErrorCode, Args...>&&>(args))...);
}

template <class CompletionHandler, class... Args>
void prepend_error_code_and_apply(CompletionHandler&& ch, detail::Tuple<Args...>&& args)
{
    detail::prepend_error_code_and_apply_impl(static_cast<CompletionHandler&&>(ch),
                                              static_cast<detail::Tuple<Args...>&&>(args),
                                              std::make_index_sequence<sizeof...(Args)>{});
}
}

AGRPC_NAMESPACE_END

#endif