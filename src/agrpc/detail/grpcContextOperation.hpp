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

#include "agrpc/detail/utility.hpp"

#include <boost/intrusive/slist_hook.hpp>

namespace agrpc::detail
{
enum class InvokeHandler
{
    YES,
    NO
};

template <bool IsIntrusivelyListable = true>
class BasicGrpcContextOperation
    : public std::conditional_t<
          IsIntrusivelyListable,
          boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::link_mode_type::normal_link>>,
          detail::Empty>

{
  public:
    constexpr void complete(bool ok, detail::InvokeHandler invoke_handler)
    {
        this->on_complete(this, ok, invoke_handler);
    }

  protected:
    using OnCompleteFunction = void (*)(BasicGrpcContextOperation*, bool, detail::InvokeHandler);

    explicit BasicGrpcContextOperation(OnCompleteFunction on_complete) noexcept : on_complete(on_complete) {}

  private:
    OnCompleteFunction on_complete;
};

using IntrusivelyListableGrpcContextOperation = detail::BasicGrpcContextOperation<true>;
using GrpcContextOperation = detail::BasicGrpcContextOperation<false>;
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP
