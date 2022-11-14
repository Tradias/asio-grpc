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

#ifndef AGRPC_DETAIL_TYPE_ERASED_OPERATION_HPP
#define AGRPC_DETAIL_TYPE_ERASED_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_context.hpp>
#include <agrpc/detail/intrusive_queue_hook.hpp>
#include <agrpc/detail/utility.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
enum class InvokeHandler
{
    NO,
    YES
};

enum class OperationResult
{
    SHUTDOWN_NOT_OK,
    SHUTDOWN_OK,
    NOT_OK,
    OK
};

struct TypeErasedOperationAccess;

class TypeErasedNoArgOperation;

using TypeErasedNoArgOnComplete = void (*)(TypeErasedNoArgOperation*, detail::OperationResult, agrpc::GrpcContext&);

class TypeErasedNoArgOperation : public detail::IntrusiveQueueHook<TypeErasedNoArgOperation>
{
  public:
    void complete(detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        this->on_complete(this, result, grpc_context);
    }

  protected:
    explicit TypeErasedNoArgOperation(TypeErasedNoArgOnComplete on_complete) noexcept : on_complete(on_complete) {}

  private:
    friend detail::TypeErasedOperationAccess;

    TypeErasedNoArgOnComplete on_complete;
};

class TypeErasedGrpcTagOperation;

using TypeErasedGrpcTagOnComplete = void (*)(TypeErasedGrpcTagOperation*, detail::OperationResult, agrpc::GrpcContext&);

class TypeErasedGrpcTagOperation

{
  public:
    void complete(detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        this->on_complete(this, result, grpc_context);
    }

  protected:
    explicit TypeErasedGrpcTagOperation(TypeErasedGrpcTagOnComplete on_complete) noexcept : on_complete(on_complete) {}

  private:
    friend detail::TypeErasedOperationAccess;

    TypeErasedGrpcTagOnComplete on_complete;
};

struct TypeErasedOperationAccess
{
    static auto& get_on_complete(detail::TypeErasedNoArgOperation& operation) noexcept { return operation.on_complete; }

    static auto get_on_complete(const detail::TypeErasedNoArgOperation& operation) noexcept
    {
        return operation.on_complete;
    }

    static auto& get_on_complete(detail::TypeErasedGrpcTagOperation& operation) noexcept
    {
        return operation.on_complete;
    }

    static auto get_on_complete(const detail::TypeErasedGrpcTagOperation& operation) noexcept
    {
        return operation.on_complete;
    }
};

[[nodiscard]] constexpr bool is_ok(OperationResult result) noexcept { return result == OperationResult::OK; }

[[nodiscard]] constexpr bool is_shutdown(OperationResult result) noexcept
{
    return result == OperationResult::SHUTDOWN_NOT_OK || result == OperationResult::SHUTDOWN_OK;
}

template <bool UseLocalAllocator, class Operation, class Base>
void do_complete_handler(Base* op, OperationResult result, agrpc::GrpcContext& grpc_context)
{
    auto* self = static_cast<Operation*>(op);
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
        if constexpr (std::is_same_v<detail::TypeErasedGrpcTagOperation, Base>)
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
    &detail::do_complete_handler<false, Operation, detail::TypeErasedNoArgOperation>;

template <class Operation>
inline constexpr auto DO_COMPLETE_LOCAL_NO_ARG_HANDLER =
    &detail::do_complete_handler<true, Operation, detail::TypeErasedNoArgOperation>;

template <class Operation>
inline constexpr auto DO_COMPLETE_GRPC_TAG_HANDLER =
    &detail::do_complete_handler<false, Operation, detail::TypeErasedGrpcTagOperation>;

template <class Operation>
inline constexpr auto DO_COMPLETE_LOCAL_GRPC_TAG_HANDLER =
    &detail::do_complete_handler<true, Operation, detail::TypeErasedGrpcTagOperation>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_TYPE_ERASED_OPERATION_HPP
