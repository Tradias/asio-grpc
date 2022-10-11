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

class TypeErasedNoArgOperation;

using TypeErasedNoArgOnComplete = void (*)(TypeErasedNoArgOperation*, detail::InvokeHandler,
                                           detail::GrpcContextLocalAllocator);

class TypeErasedNoArgOperation : public detail::IntrusiveQueueHook<TypeErasedNoArgOperation>
{
  public:
    void complete(detail::InvokeHandler invoke_handler, detail::GrpcContextLocalAllocator allocator)
    {
        this->on_complete(this, invoke_handler, allocator);
    }

  protected:
    explicit TypeErasedNoArgOperation(TypeErasedNoArgOnComplete on_complete) noexcept : on_complete(on_complete) {}

  private:
    TypeErasedNoArgOnComplete on_complete;
};

class TypeErasedGrpcTagOperation;

using TypeErasedGrpcTagOnComplete = void (*)(TypeErasedGrpcTagOperation*, detail::InvokeHandler, bool,
                                             detail::GrpcContextLocalAllocator);

class TypeErasedGrpcTagOperation

{
  public:
    void complete(detail::InvokeHandler invoke_handler, bool ok, detail::GrpcContextLocalAllocator allocator)
    {
        this->on_complete(this, invoke_handler, ok, allocator);
    }

  protected:
    explicit TypeErasedGrpcTagOperation(TypeErasedGrpcTagOnComplete on_complete) noexcept : on_complete(on_complete) {}

  private:
    TypeErasedGrpcTagOnComplete on_complete;
};

template <bool UseLocalAllocator, class Operation, class Base, class... Args>
void do_complete_handler(Base* op, detail::InvokeHandler invoke_handler, Args... args,
                         detail::GrpcContextLocalAllocator allocator)
{
    auto* self = static_cast<Operation*>(op);
    detail::AllocationGuard ptr{self, [&]
                                {
                                    if constexpr (UseLocalAllocator)
                                    {
                                        return allocator;
                                    }
                                    else
                                    {
                                        return self->get_allocator();
                                    }
                                }()};
    if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
    {
        auto handler{std::move(self->completion_handler())};
        ptr.reset();
        std::move(handler)(static_cast<Args&&>(args)...);
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
    &detail::do_complete_handler<false, Operation, detail::TypeErasedGrpcTagOperation, bool>;

template <class Operation>
inline constexpr auto DO_COMPLETE_LOCAL_GRPC_TAG_HANDLER =
    &detail::do_complete_handler<true, Operation, detail::TypeErasedGrpcTagOperation, bool>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_TYPE_ERASED_OPERATION_HPP
