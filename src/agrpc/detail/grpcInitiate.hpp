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

#ifndef AGRPC_DETAIL_GRPCINITIATE_HPP
#define AGRPC_DETAIL_GRPCINITIATE_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpcSender.hpp>
#include <agrpc/detail/useSender.hpp>
#include <agrpc/detail/utility.hpp>

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/defaultCompletionToken.hpp>
#include <agrpc/detail/grpcInitiator.hpp>
#endif

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class StopFunction>
using GrpcInitiateTemplateArgs = void (*)(StopFunction);

struct GrpcInitiateImplFn
{
    GrpcInitiateImplFn() = default;
    GrpcInitiateImplFn(const GrpcInitiateImplFn&) = delete;
    GrpcInitiateImplFn(GrpcInitiateImplFn&&) = delete;
    GrpcInitiateImplFn& operator=(const GrpcInitiateImplFn&) = delete;
    GrpcInitiateImplFn& operator=(GrpcInitiateImplFn&&) = delete;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class StopFunction, class InitiatingFunction, class CompletionToken>
    auto operator()(const detail::GrpcInitiateTemplateArgs<StopFunction>, InitiatingFunction initiating_function,
                    CompletionToken token) const
    {
        return asio::async_initiate<CompletionToken, void(bool)>(
            detail::GrpcInitiator<InitiatingFunction, StopFunction>{std::move(initiating_function)}, token);
    }
#endif

    template <class StopFunction, class InitiatingFunction>
    auto operator()(const detail::GrpcInitiateTemplateArgs<StopFunction>, InitiatingFunction initiating_function,
                    detail::UseSender token) const noexcept
    {
        return detail::GrpcSender<InitiatingFunction, StopFunction>{token.grpc_context, std::move(initiating_function)};
    }
};

inline constexpr detail::GrpcInitiateImplFn grpc_initiate_impl{};

template <class InitiatingFunction, class CompletionToken>
auto grpc_initiate(InitiatingFunction&& initiating_function, CompletionToken&& token)
{
    return detail::grpc_initiate_impl(detail::GrpcInitiateTemplateArgs<detail::Empty>{},
                                      std::forward<InitiatingFunction>(initiating_function),
                                      std::forward<CompletionToken>(token));
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class Payload, class InitiatingFunction, class CompletionToken>
auto grpc_initiate_with_payload(InitiatingFunction initiating_function, CompletionToken token)
{
    return asio::async_initiate<CompletionToken, void(std::pair<Payload, bool>)>(
        detail::GrpcWithPayloadInitiator<Payload, InitiatingFunction>{std::move(initiating_function)}, token);
}
#endif

template <class CompletionToken>
inline constexpr bool IS_NOTRHOW_GRPC_INITIATE_COMPLETION_TOKEN =
    std::is_same_v<detail::UseSender, detail::RemoveCrefT<CompletionToken>>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCINITIATE_HPP
