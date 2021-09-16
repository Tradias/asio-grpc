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

#ifndef AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP

#include "agrpc/detail/grpcContext.hpp"
#include "agrpc/detail/utility.hpp"

#include <boost/intrusive/slist_hook.hpp>

namespace agrpc::detail
{
enum class InvokeHandler
{
    YES,
    NO
};

template <bool IsIntrusivelyListable, class... Signature>
class TypeErasedOperation
    : public std::conditional_t<
          IsIntrusivelyListable,
          boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::link_mode_type::normal_link>>,
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

using TypeErasedNoArgLocalOperation = detail::TypeErasedOperation<true, detail::GrpcContextLocalAllocator>;
using TypeErasedNoArgRemoteOperation = detail::TypeErasedOperation<false>;
using TypeErasedGrpcTagOperation = detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>;
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP
