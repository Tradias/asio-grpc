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

#ifndef AGRPC_DETAIL_GRPC_SENDER_HPP
#define AGRPC_DETAIL_GRPC_SENDER_HPP

#include <agrpc/detail/basic_sender.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct GrpcSenderImplementationBase
{
    static constexpr auto TYPE = detail::SenderImplementationType::GRPC_TAG;
    static constexpr bool NEEDS_ON_COMPLETE = false;

    using Signature = void(bool);
    using StopFunction = detail::Empty;

    static void complete(const agrpc::GrpcContext&, bool) noexcept {}
};

template <class StopFunctionT = detail::Empty>
struct GrpcSenderImplementation : detail::GrpcSenderImplementationBase
{
    using StopFunction = StopFunctionT;
};

template <class InitiatingFunction>
struct GrpcSenderInitiation
{
    auto& stop_function_arg() const noexcept { return initiating_function_; }

    void initiate(agrpc::GrpcContext& grpc_context, void* tag) const { initiating_function_(grpc_context, tag); }

    InitiatingFunction initiating_function_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SENDER_HPP
