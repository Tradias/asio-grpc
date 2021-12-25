// Copyright 2021 Dennis Hezel
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

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/defaultCompletionToken.hpp"
#include "agrpc/detail/grpcSender.hpp"
#include "agrpc/detail/initiate.hpp"
#include "agrpc/detail/utility.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct GrpcInitiateImplFn
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    template <class InitiatingFunction, class CompletionToken = detail::DefaultCompletionToken,
              class StopFunction = detail::Empty>
    auto operator()(InitiatingFunction initiating_function, CompletionToken token = {},
                    StopFunction stop_function = {}) const
    {
#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
        if constexpr (!std::is_same_v<detail::Empty, StopFunction>)
        {
            if (auto cancellation_slot = asio::get_associated_cancellation_slot(token);
                cancellation_slot.is_connected())
            {
                cancellation_slot.assign(std::move(stop_function));
            }
        }
#endif
        return asio::async_initiate<CompletionToken, void(bool)>(detail::GrpcInitiator{std::move(initiating_function)},
                                                                 token);
    }
#endif

    template <class InitiatingFunction, class StopFunction = detail::Empty>
    auto operator()(InitiatingFunction initiating_function, detail::UseSender token, StopFunction = {}) const
    {
        return detail::GrpcSender<InitiatingFunction, StopFunction>{token.grpc_context, std::move(initiating_function)};
    }
};

inline constexpr detail::GrpcInitiateImplFn grpc_initiate{};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCINITIATE_HPP
