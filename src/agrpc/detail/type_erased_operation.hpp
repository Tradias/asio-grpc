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

template <bool IsIntrusivelyListable, class... Signature>
class TypeErasedOperation
    : public detail::ConditionalT<IsIntrusivelyListable,
                                  detail::IntrusiveQueueHook<TypeErasedOperation<IsIntrusivelyListable, Signature...>>,
                                  detail::Empty>

{
  public:
    using OnComplete = void (*)(TypeErasedOperation*, detail::InvokeHandler, Signature...);

    void complete(detail::InvokeHandler invoke_handler, Signature... args)
    {
        this->on_complete(this, invoke_handler, static_cast<Signature&&>(args)...);
    }

  protected:
    explicit TypeErasedOperation(OnComplete on_complete) noexcept : on_complete(on_complete) {}

  private:
    OnComplete on_complete;
};

using TypeErasedNoArgOperation = detail::TypeErasedOperation<true, detail::GrpcContextLocalAllocator>;
using TypeErasedGrpcTagOperation = detail::TypeErasedOperation<false, bool, detail::GrpcContextLocalAllocator>;

using TypeErasedNoArgOnComplete = detail::TypeErasedNoArgOperation::OnComplete;
using TypeErasedGrpcTagOnComplete = detail::TypeErasedGrpcTagOperation::OnComplete;

template <class Operation, class Base, class... Args>
void default_do_complete(Base* op, detail::InvokeHandler invoke_handler, Args... args,
                         detail::GrpcContextLocalAllocator)
{
    auto* self = static_cast<Operation*>(op);
    detail::AllocationGuard ptr{self, self->get_allocator()};
    if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
    {
        auto handler{std::move(self->completion_handler())};
        ptr.reset();
        std::move(handler)(static_cast<Args&&>(args)...);
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_TYPE_ERASED_OPERATION_HPP
