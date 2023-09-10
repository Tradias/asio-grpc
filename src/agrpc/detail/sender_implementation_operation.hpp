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

#ifndef AGRPC_DETAIL_SENDER_IMPLEMENTATION_OPERATION_HPP
#define AGRPC_DETAIL_SENDER_IMPLEMENTATION_OPERATION_HPP

#include <agrpc/detail/allocate_operation.hpp>
#include <agrpc/detail/allocation_type.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_implementation.hpp>
#include <agrpc/detail/operation_initiation.hpp>
#include <agrpc/detail/receiver_and_stop_callback.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ImplementationT, class CompletionHandler>
struct SenderImplementationOperation : public detail::BaseForSenderImplementationTypeT<ImplementationT::TYPE>
{
    using Implementation = ImplementationT;
    using Base = detail::BaseForSenderImplementationTypeT<Implementation::TYPE>;
    using StopFunction = typename Implementation::StopFunction;
    using StopToken = exec::stop_token_type_t<CompletionHandler&>;

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

    template <class Initation>
    SenderImplementationOperation(detail::AllocationType allocation_type, CompletionHandler&& completion_handler,
                                  agrpc::GrpcContext& grpc_context, const Initation& initiation,
                                  Implementation&& implementation)
        : Base(get_on_complete(allocation_type)),
          impl_(static_cast<CompletionHandler&&>(completion_handler), static_cast<Implementation&&>(implementation))
    {
        grpc_context.work_started();
        emplace_stop_callback(initiation);
        detail::initiate<detail::DeallocateOnComplete::YES>(*this, grpc_context, initiation, allocation_type);
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
            return exec::get_allocator(completion_handler());
        }
    }

    template <class Initiation>
    void emplace_stop_callback(const Initiation& initiation)
    {
        detail::emplace_stop_callback<StopFunction>(*this,
                                                    [&](auto...) -> decltype(auto)
                                                    {
                                                        return detail::get_stop_function_arg(initiation,
                                                                                             implementation());
                                                    });
    }

    CompletionHandler& completion_handler() noexcept { return impl_.first(); }

    Implementation& implementation() noexcept { return impl_.second(); }

    Base* tag() noexcept { return this; }

    template <AllocationType AllocType, int Id>
    void set_on_complete() noexcept
    {
        detail::OperationBaseAccess::get_on_complete(*this) = &do_complete<AllocType, Id>;
    }

    template <AllocationType AllocType, class... Args>
    void complete(agrpc::GrpcContext& grpc_context, Args... args)
    {
        detail::AllocationGuard ptr{this, get_allocator<AllocType>(grpc_context)};
        auto handler{static_cast<CompletionHandler&&>(completion_handler())};
        ptr.reset();
        static_cast<CompletionHandler&&>(handler)(static_cast<Args&&>(args)...);
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
