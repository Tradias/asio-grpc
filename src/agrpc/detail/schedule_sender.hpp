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

#ifndef AGRPC_DETAIL_SCHEDULE_SENDER_HPP
#define AGRPC_DETAIL_SCHEDULE_SENDER_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/detail/grpc_sender.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct ScheduleSenderImplementation
{
    static constexpr auto TYPE = detail::SenderImplementationType::NO_ARG;

    using Signature = void();
    using StopFunction = detail::Empty;

    void initiate(agrpc::GrpcContext& grpc_context, detail::TypeErasedNoArgOperation* self)
    {
        detail::GrpcContextImplementation::add_operation(grpc_context, self);
    }

    template <class OnDone>
    void done(OnDone on_done)
    {
        on_done();
    }
};

using ScheduleSender = detail::BasicGrpcSender<detail::ScheduleSenderImplementation>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SCHEDULE_SENDER_HPP
