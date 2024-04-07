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

#ifndef AGRPC_DETAIL_OPERATION_BASE_HPP
#define AGRPC_DETAIL_OPERATION_BASE_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_context.hpp>
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
enum class OperationResult
{
    SHUTDOWN_NOT_OK,
    SHUTDOWN_OK,
    NOT_OK,
    OK_
};

struct OperationBaseAccess;

class OperationBase;

using OperationOnComplete = void (*)(detail::OperationBase*, detail::OperationResult, agrpc::GrpcContext&);

class OperationBase
{
  public:
    void complete(detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        on_complete_(this, result, grpc_context);
    }

  protected:
    explicit OperationBase(OperationOnComplete on_complete) noexcept : on_complete_(on_complete) {}

  private:
    friend detail::OperationBaseAccess;

    union
    {
        OperationOnComplete on_complete_;
        void* scratch_space_;
    };
};

class QueueableOperationBase : public detail::OperationBase
{
  protected:
    using detail::OperationBase::OperationBase;

  private:
    friend detail::OperationBaseAccess;
    friend detail::IntrusiveQueue<QueueableOperationBase>;
    friend detail::AtomicIntrusiveQueue<QueueableOperationBase>;

    QueueableOperationBase* next_;
};

struct OperationBaseAccess
{
    static void set_on_complete(detail::OperationBase& operation, OperationOnComplete on_complete) noexcept
    {
        operation.on_complete_ = on_complete;
    }

    static auto get_on_complete(const detail::OperationBase& operation) noexcept { return operation.on_complete_; }

    static void set_scratch_space(detail::OperationBase& operation, void* ptr) noexcept
    {
        operation.scratch_space_ = ptr;
    }

    static void* get_scratch_space(const detail::OperationBase& operation) noexcept { return operation.scratch_space_; }
};

[[nodiscard]] constexpr bool is_ok(OperationResult result) noexcept { return result == OperationResult::OK_; }

[[nodiscard]] constexpr bool is_shutdown(OperationResult result) noexcept
{
    return result == OperationResult::SHUTDOWN_NOT_OK || result == OperationResult::SHUTDOWN_OK;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_BASE_HPP
