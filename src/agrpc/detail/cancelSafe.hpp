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

#ifndef AGRPC_DETAIL_CANCELSAFE_HPP
#define AGRPC_DETAIL_CANCELSAFE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/typeErasedCompletionHandler.hpp"
#include "agrpc/detail/workTrackingCompletionHandler.hpp"

#include <tuple>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Signature>
struct PrependErrorCodeToSignature;

template <class... Args>
struct PrependErrorCodeToSignature<void(detail::ErrorCode, Args...)>
{
    using Type = void(detail::ErrorCode, Args...);
};

template <class... Args>
struct PrependErrorCodeToSignature<void(Args...)>
{
    using Type = void(detail::ErrorCode, Args...);
};

template <class Signature>
using PrependErrorCodeToSignatureT = typename detail::PrependErrorCodeToSignature<Signature>::Type;

template <class CompletionHandler, class... Args>
void complete_successfully(CompletionHandler&& handler, detail::ErrorCode ec, Args&&... args)
{
    std::forward<CompletionHandler>(handler).complete(ec, std::forward<Args>(args)...);
}

template <class CompletionHandler, class... Args>
void complete_successfully(CompletionHandler&& handler, Args&&... args)
{
    std::forward<CompletionHandler>(handler).complete(detail::ErrorCode{}, std::forward<Args>(args)...);
}

template <class CompletionHandler, class... Args>
void invoke_successfully_from_tuple(CompletionHandler&& handler, std::tuple<detail::ErrorCode, Args...>&& args)
{
    std::apply(std::forward<CompletionHandler>(handler), std::move(args));
}

template <class CompletionHandler, class... Args>
void invoke_successfully_from_tuple(CompletionHandler&& handler, std::tuple<Args...>&& args)
{
    std::apply(std::forward<CompletionHandler>(handler),
               std::tuple_cat(std::forward_as_tuple(detail::ErrorCode{}), std::move(args)));
}

template <class CompletionHandler, class... Args>
void post_complete_operation_aborted(CompletionHandler&& handler, detail::ErrorCode, Args&&... args)
{
    std::forward<CompletionHandler>(handler).post_complete(asio::error::operation_aborted, std::forward<Args>(args)...);
}

template <class CompletionHandler, class... Args>
void post_complete_operation_aborted(CompletionHandler&& handler, Args&&... args)
{
    std::forward<CompletionHandler>(handler).post_complete(asio::error::operation_aborted, std::forward<Args>(args)...);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CANCELSAFE_HPP
