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

#ifndef AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP
#define AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP

#include <agrpc/detail/type_erased_operation.hpp>

#include <type_traits>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
enum class SenderImplementationType
{
    NO_ARG,
    GRPC_TAG,
    BOTH
};

struct BasicSenderRunningOperationBaseArg
{
    detail::TypeErasedNoArgOnComplete no_arg_on_complete;
    detail::TypeErasedGrpcTagOnComplete grpc_tag_on_complete;
};

template <detail::SenderImplementationType>
struct BasicSenderRunningOperationBase;

template <>
struct BasicSenderRunningOperationBase<detail::SenderImplementationType::NO_ARG> : detail::TypeErasedNoArgOperation
{
    explicit BasicSenderRunningOperationBase(detail::BasicSenderRunningOperationBaseArg arg) noexcept
        : detail::TypeErasedNoArgOperation(arg.no_arg_on_complete)
    {
    }

    void set_on_complete(detail::BasicSenderRunningOperationBaseArg arg) noexcept
    {
        detail::TypeErasedOperationAccess::get_on_complete(*this) = arg.no_arg_on_complete;
    }
};

template <>
struct BasicSenderRunningOperationBase<detail::SenderImplementationType::GRPC_TAG> : detail::TypeErasedGrpcTagOperation
{
    explicit BasicSenderRunningOperationBase(detail::BasicSenderRunningOperationBaseArg arg) noexcept
        : detail::TypeErasedGrpcTagOperation(arg.grpc_tag_on_complete)
    {
    }

    void set_on_complete(detail::BasicSenderRunningOperationBaseArg arg) noexcept
    {
        detail::TypeErasedOperationAccess::get_on_complete(*this) = arg.grpc_tag_on_complete;
    }
};

template <>
struct BasicSenderRunningOperationBase<detail::SenderImplementationType::BOTH> : detail::TypeErasedNoArgOperation,
                                                                                 detail::TypeErasedGrpcTagOperation

{
    explicit BasicSenderRunningOperationBase(detail::BasicSenderRunningOperationBaseArg arg) noexcept
        : detail::TypeErasedNoArgOperation(arg.no_arg_on_complete),
          detail::TypeErasedGrpcTagOperation(arg.grpc_tag_on_complete)
    {
    }

    void set_on_complete(detail::BasicSenderRunningOperationBaseArg arg) noexcept
    {
        detail::TypeErasedOperationAccess::get_on_complete(*static_cast<detail::TypeErasedNoArgOperation*>(this)) =
            arg.no_arg_on_complete;
        detail::TypeErasedOperationAccess::get_on_complete(*static_cast<detail::TypeErasedGrpcTagOperation*>(this)) =
            arg.grpc_tag_on_complete;
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP
