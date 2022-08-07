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

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/sender_allocation_traits.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Implementation, class Default, class = void>
struct GetStopFunction
{
    using Type = Default;
};

template <class Implementation, class Default>
struct GetStopFunction<Implementation, Default, std::void_t<typename Implementation::StopFunction>>
{
    using Type = typename Implementation::StopFunction;
};

template <class Implementation, class Default>
using GetStopFunctionT = typename detail::GetStopFunction<Implementation, Default>::Type;

template <class Implementation, class Default, class = void>
struct GetSignature
{
    using Type = Default;
};

template <class Implementation, class Default>
struct GetSignature<Implementation, Default, std::void_t<typename Implementation::Signature>>
{
    using Type = typename Implementation::Signature;
};

template <class Implementation, class Default>
using GetSignatureT = typename detail::GetSignature<Implementation, Default>::Type;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_IMPLEMENTATION_HPP
