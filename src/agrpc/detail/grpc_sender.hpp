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

#ifndef AGRPC_DETAIL_GRPC_SENDER_HPP
#define AGRPC_DETAIL_GRPC_SENDER_HPP

#include <agrpc/detail/basic_sender.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct GrpcSenderImplementationBase
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;

    using Signature = void(bool);
    using StopFunction = detail::Empty;
};

template <class InitiatingFunction, class StopFunctionT>
struct GrpcSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = StopFunctionT;

    struct Initiation
    {
        auto create_stop_function() noexcept { return StopFunction{initiating_function}; }

        InitiatingFunction initiating_function;
    };

    template <class OnDone>
    void initiate(agrpc::GrpcContext& grpc_context, OnDone on_done)
    {
        on_done->initiating_function(grpc_context, on_done.self());
    }

    template <class OnDone>
    void done(OnDone on_done, bool ok)
    {
        on_done(ok);
    }
};

template <class InitiatingFunction, class StopFunction = detail::Empty>
using GrpcSender = detail::BasicSender<detail::GrpcSenderImplementation<InitiatingFunction, StopFunction>>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SENDER_HPP
