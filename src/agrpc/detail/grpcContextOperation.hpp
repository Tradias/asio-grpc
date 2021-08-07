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

#include <boost/intrusive/slist_hook.hpp>

namespace agrpc::detail
{
class GrpcContextOperation : public boost::intrusive::slist_base_hook<
                                 boost::intrusive::link_mode<boost::intrusive::link_mode_type::normal_link>>

{
  public:
    enum class InvokeHandler
    {
        YES,
        NO
    };

    constexpr void complete(bool ok, InvokeHandler invoke_handler) { on_complete(this, ok, invoke_handler); }

  protected:
    using OnCompleteFunction = void (*)(GrpcContextOperation*, bool, InvokeHandler);

    OnCompleteFunction on_complete;

    explicit GrpcContextOperation(OnCompleteFunction on_complete) noexcept : on_complete(on_complete) {}
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTOPERATION_HPP
