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

#ifndef AGRPC_DETAIL_SENDER_IMPLEMENTATION_OPERATION_HPP
#define AGRPC_DETAIL_SENDER_IMPLEMENTATION_OPERATION_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_implementation.hpp>
#include <agrpc/detail/operation_initiation.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/detail/work_tracking_completion_handler.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ImplementationT, class CompletionHandler>
struct SenderImplementationOperation : public detail::BaseForSenderImplementationTypeT<ImplementationT::TYPE>,
                                       private detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandler>>
{
    using Implementation = ImplementationT;
    using Base = detail::BaseForSenderImplementationTypeT<Implementation::TYPE>;
    using WorkTracker = detail::WorkTracker<detail::AssociatedExecutorT<CompletionHandler>>;
    using StopFunction = typename Implementation::StopFunction;

    template <detail::AllocationType AllocType, int Id = 0>
    static void do_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        auto* self = static_cast<SenderImplementationOperation*>(op);
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            detail::complete<AllocType, Id>(*self, result, grpc_context);
        }
        else
        {
            [[maybe_unused]] detail::AllocationGuard ptr{self, self->template get_allocator<AllocType>(grpc_context)};
        }
    }

    static detail::OperationOnComplete get_on_complete(detail::AllocationType allocation_type)
    {
        if (allocation_type == detail::AllocationType::LOCAL)
        {
            return &do_complete<detail::AllocationType::LOCAL>;
        }
        return &do_complete<detail::AllocationType::CUSTOM>;
    }

    template <class Ch, class Initation>
    SenderImplementationOperation(detail::AllocationType allocation_type, Ch&& completion_handler,
                                  agrpc::GrpcContext& grpc_context, const Initation& initiation,
                                  Implementation&& implementation)
        : Base(get_on_complete(allocation_type)),
          WorkTracker(asio::get_associated_executor(completion_handler)),
          impl_(static_cast<Ch&&>(completion_handler), static_cast<Implementation&&>(implementation))
    {
        grpc_context.work_started();
        emplace_stop_callback(initiation);
        detail::initiate<detail::DeallocateOnComplete::YES_>(*this, grpc_context, initiation, allocation_type);
    }

    template <AllocationType AllocType>
    decltype(auto) get_allocator(agrpc::GrpcContext& grpc_context) noexcept
    {
        if constexpr (AllocType == detail::AllocationType::LOCAL)
        {
            return grpc_context.get_allocator();
        }
        else
        {
            return asio::get_associated_allocator(completion_handler());
        }
    }

    template <class Initiation>
    void emplace_stop_callback(const Initiation& initiation)
    {
        if constexpr (detail::NEEDS_STOP_CALLBACK<detail::CancellationSlotT<CompletionHandler&>, StopFunction>)
        {
            if (auto slot = detail::get_cancellation_slot(completion_handler()); slot.is_connected())
            {
                slot.template emplace<StopFunction>(detail::get_stop_function_arg(initiation, implementation()));
            }
        }
    }

    CompletionHandler& completion_handler() noexcept { return impl_.first(); }

    WorkTracker& work_tracker() noexcept { return *this; }

    Implementation& implementation() noexcept { return impl_.second(); }

    Base* tag() noexcept { return this; }

    template <AllocationType AllocType, int Id>
    void set_on_complete() noexcept
    {
        detail::OperationBaseAccess::set_on_complete(*this, &do_complete<AllocType, Id>);
    }

    template <AllocationType AllocType, class... Args>
    void complete(agrpc::GrpcContext& grpc_context, Args... args)
    {
        detail::AllocationGuard ptr{this, get_allocator<AllocType>(grpc_context)};
        detail::dispatch_complete(ptr, static_cast<Args&&>(args)...);
    }

    detail::CompressedPair<CompletionHandler, Implementation> impl_;
};

template <class Implementation>
struct SenderImplementationOperationTemplate
{
    template <class CompletionHandler>
    using Type = detail::SenderImplementationOperation<Implementation, CompletionHandler>;
};

template <class CompletionHandler, class Initiation, class Implementation>
void submit_sender_implementation_operation(agrpc::GrpcContext& grpc_context, CompletionHandler&& completion_handler,
                                            const Initiation& initiation, Implementation&& implementation)
{
    if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
    {
        return;
    }
    detail::allocate_operation<
        detail::SenderImplementationOperationTemplate<detail::RemoveCrefT<Implementation>>::template Type>(
        grpc_context, static_cast<CompletionHandler&&>(completion_handler), grpc_context, initiation,
        static_cast<Implementation&&>(implementation));
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_IMPLEMENTATION_OPERATION_HPP
