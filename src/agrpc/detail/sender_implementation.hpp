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

template <class Implementation, class Default, detail::SenderImplementationType = Implementation::TYPE>
struct GetNoArgTypeErasedBase
{
    using Type = Default;
};

template <class Implementation, class Default>
struct GetNoArgTypeErasedBase<Implementation, Default, detail::SenderImplementationType::NO_ARG>
{
    using Type = detail::TypeErasedNoArgOperation;
};

template <class Implementation, class Default>
struct GetNoArgTypeErasedBase<Implementation, Default, detail::SenderImplementationType::BOTH>
{
    using Type = detail::TypeErasedNoArgOperation;
};

template <class Implementation, class Default>
using GetNoArgTypeErasedBaseT = typename detail::GetNoArgTypeErasedBase<Implementation, Default>::Type;

template <class Implementation, class Default, detail::SenderImplementationType = Implementation::TYPE>
struct GetGrpcTagTypeErasedBase
{
    using Type = Default;
};

template <class Implementation, class Default>
struct GetGrpcTagTypeErasedBase<Implementation, Default, detail::SenderImplementationType::GRPC_TAG>
{
    using Type = detail::TypeErasedGrpcTagOperation;
};

template <class Implementation, class Default>
struct GetGrpcTagTypeErasedBase<Implementation, Default, detail::SenderImplementationType::BOTH>
{
    using Type = detail::TypeErasedGrpcTagOperation;
};

template <class Implementation, class Default>
using GetGrpcTagTypeErasedBaseT = typename detail::GetGrpcTagTypeErasedBase<Implementation, Default>::Type;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP
