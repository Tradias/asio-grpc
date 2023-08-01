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

#ifndef AGRPC_DETAIL_OPERATION_HPP
#define AGRPC_DETAIL_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/operation_base.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Handler>
class NoArgOperation : public detail::QueueableOperationBase
{
  private:
    using Base = detail::QueueableOperationBase;

  public:
    template <class... Args>
    explicit NoArgOperation(detail::AllocationType allocation_type, Args&&... args)
        : Base(detail::AllocationType::LOCAL == allocation_type
                   ? detail::DO_COMPLETE_LOCAL_NO_ARG_HANDLER<NoArgOperation>
                   : detail::DO_COMPLETE_NO_ARG_HANDLER<NoArgOperation>),
          handler_(static_cast<Args&&>(args)...)
    {
    }

    [[nodiscard]] Handler& completion_handler() noexcept { return handler_; }

    [[nodiscard]] auto get_allocator() noexcept { return detail::exec::get_allocator(handler_); }

  private:
    Handler handler_;
};

template <class Handler>
class GrpcTagOperation : public detail::OperationBase
{
  private:
    using Base = detail::OperationBase;

  public:
    template <class... Args>
    explicit GrpcTagOperation(detail::AllocationType allocation_type, Args&&... args)
        : Base(detail::AllocationType::LOCAL == allocation_type
                   ? detail::DO_COMPLETE_LOCAL_GRPC_TAG_HANDLER<GrpcTagOperation>
                   : detail::DO_COMPLETE_GRPC_TAG_HANDLER<GrpcTagOperation>),
          handler_(static_cast<Args&&>(args)...)
    {
    }

    [[nodiscard]] Handler& completion_handler() noexcept { return handler_; }

    [[nodiscard]] auto get_allocator() noexcept { return detail::exec::get_allocator(handler_); }

  private:
    Handler handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_HPP
