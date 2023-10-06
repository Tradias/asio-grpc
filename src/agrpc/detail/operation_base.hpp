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
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_context.hpp>
#include <agrpc/detail/utility.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
enum class OperationResult
{
    SHUTDOWN_NOT_OK,
    SHUTDOWN_OK,
    NOT_OK,
    OK
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

[[nodiscard]] constexpr bool is_ok(OperationResult result) noexcept { return result == OperationResult::OK; }

[[nodiscard]] constexpr bool is_shutdown(OperationResult result) noexcept
{
    return result == OperationResult::SHUTDOWN_NOT_OK || result == OperationResult::SHUTDOWN_OK;
}

template <bool UseLocalAllocator, class Operation, class Base>
void do_complete_handler(detail::OperationBase* op, OperationResult result, agrpc::GrpcContext& grpc_context)
{
    auto* self = static_cast<Operation*>(static_cast<Base*>(op));
    detail::AllocationGuard ptr{self, [&]
                                {
                                    if constexpr (UseLocalAllocator)
                                    {
                                        return detail::get_local_allocator(grpc_context);
                                    }
                                    else
                                    {
                                        return self->get_allocator();
                                    }
                                }()};
    if AGRPC_LIKELY (!detail::is_shutdown(result))
    {
        auto handler{std::move(self->completion_handler())};
        ptr.reset();
        if constexpr (std::is_same_v<detail::OperationBase, Base>)
        {
            std::move(handler)(detail::is_ok(result));
        }
        else
        {
            std::move(handler)();
        }
    }
}

template <class Operation>
inline constexpr auto DO_COMPLETE_NO_ARG_HANDLER =
    &detail::do_complete_handler<false, Operation, detail::QueueableOperationBase>;

template <class Operation>
inline constexpr auto DO_COMPLETE_LOCAL_NO_ARG_HANDLER =
    &detail::do_complete_handler<true, Operation, detail::QueueableOperationBase>;

template <class Operation>
inline constexpr auto DO_COMPLETE_GRPC_TAG_HANDLER =
    &detail::do_complete_handler<false, Operation, detail::OperationBase>;

template <class Operation>
inline constexpr auto DO_COMPLETE_LOCAL_GRPC_TAG_HANDLER =
    &detail::do_complete_handler<true, Operation, detail::OperationBase>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OPERATION_BASE_HPP
