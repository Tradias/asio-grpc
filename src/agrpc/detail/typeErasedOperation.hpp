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

#ifndef AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP

#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcContext.hpp"
#include "agrpc/detail/intrusiveQueueHook.hpp"
#include "agrpc/detail/utility.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
enum class InvokeHandler
{
    YES,
    NO
};

template <bool IsIntrusivelyListable, class... Signature>
class TypeErasedOperation
    : public std::conditional_t<IsIntrusivelyListable,
                                detail::IntrusiveQueueHook<TypeErasedOperation<IsIntrusivelyListable, Signature...>>,
                                detail::Empty>

{
  public:
    constexpr void complete(detail::InvokeHandler invoke_handler, Signature... args)
    {
        this->on_complete(this, invoke_handler, detail::forward_as<Signature>(args)...);
    }

  protected:
    using OnCompleteFunction = void (*)(TypeErasedOperation*, detail::InvokeHandler, Signature...);

    explicit TypeErasedOperation(OnCompleteFunction on_complete) noexcept : on_complete(on_complete) {}

  private:
    OnCompleteFunction on_complete;
};

using TypeErasedNoArgOperation = detail::TypeErasedOperation<true, detail::GrpcContextLocalAllocator>;
using TypeErasedGrpcTagOperation = detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP
