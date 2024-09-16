// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_NO_ARG_OPERATION_HPP
#define AGRPC_DETAIL_NO_ARG_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/association.hpp>
#include <agrpc/detail/grpc_context_local_allocator.hpp>
#include <agrpc/detail/operation_base.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Handler>
class NoArgOperation : public detail::NoArgOperationBase
{
  private:
    using Base = detail::NoArgOperationBase;

    template <bool UseLocalAllocator>
    static void do_complete(detail::OperationBase* op, OperationResult result, agrpc::GrpcContext&)
    {
        auto& self = *static_cast<NoArgOperation*>(op);
        detail::AllocationGuard ptr{self, [&]
                                    {
                                        if constexpr (UseLocalAllocator)
                                        {
                                            return detail::get_local_allocator();
                                        }
                                        else
                                        {
                                            return detail::get_allocator(self.handler_);
                                        }
                                    }()};
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            auto handler{std::move(self.handler_)};
            ptr.reset();
            std::move(handler)();
        }
    }

  public:
    template <class... Args>
    explicit NoArgOperation(detail::AllocationType allocation_type, Args&&... args)
        : Base(detail::AllocationType::LOCAL == allocation_type ? do_complete<true> : do_complete<false>),
          handler_(static_cast<Args&&>(args)...)
    {
    }

  private:
    Handler handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NO_ARG_OPERATION_HPP
