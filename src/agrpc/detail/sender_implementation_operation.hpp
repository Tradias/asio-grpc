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
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Initiation, class Implementation>
auto get_stop_function_arg(const Initiation& initiation, Implementation& implementation)
    -> decltype(initiation.stop_function_arg(implementation))
{
    return initiation.stop_function_arg(implementation);
}

template <class Initiation, class Implementation>
auto get_stop_function_arg(const Initiation& initiation, const Implementation&)
    -> decltype(initiation.stop_function_arg())
{
    return initiation.stop_function_arg();
}

template <class Implementation, class CompletionHandler>
struct SenderImplementationOperation : public detail::BaseForSenderImplementationTypeT<Implementation::TYPE>
{
    using Base = detail::BaseForSenderImplementationTypeT<Implementation::TYPE>;
    using StopFunction = typename Implementation::StopFunction;
    using StopToken = exec::stop_token_type_t<CompletionHandler&>;

    template <detail::AllocationType AllocType>
    static void do_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext& grpc_context)
    {
        auto* self = static_cast<SenderImplementationOperation*>(op);
        detail::AllocationGuard ptr{self, [&]
                                    {
                                        if constexpr (AllocType == detail::AllocationType::LOCAL)
                                        {
                                            return grpc_context.get_allocator();
                                        }
                                        else
                                        {
                                            return exec::get_allocator(self->completion_handler());
                                        }
                                    }()};
        if AGRPC_LIKELY (!detail::is_shutdown(result))
        {
            if constexpr (Implementation::TYPE == detail::SenderImplementationType::BOTH ||
                          Implementation::TYPE == detail::SenderImplementationType::GRPC_TAG)
            {
                self->implementation().done(grpc_context, detail::is_ok(result));
            }
            else
            {
                self->implementation().done(grpc_context);
            }
            auto handler{static_cast<CompletionHandler&&>(self->completion_handler())};
            ptr.reset();
            if constexpr (Implementation::TYPE == detail::SenderImplementationType::BOTH ||
                          Implementation::TYPE == detail::SenderImplementationType::GRPC_TAG)
            {
                static_cast<CompletionHandler&&>(handler)(detail::is_ok(result));
            }
            else
            {
                static_cast<CompletionHandler&&>(handler)();
            }
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

    Implementation& implementation() noexcept { return impl_.second(); }

    template <class Initiation>
    void emplace_stop_callback(const Initiation& initiation) noexcept
    {
        if constexpr (detail::NEEDS_STOP_CALLBACK<exec::stop_token_type_t<CompletionHandler&>, StopFunction>)
        {
            auto stop_token = exec::get_stop_token(this->completion_handler());
            if (detail::stop_possible(stop_token))
            {
                stop_token.template emplace<StopFunction>(detail::get_stop_function_arg(initiation, implementation()));
            }
        }
    }

    SenderImplementationOperation(detail::AllocationType allocation_type, CompletionHandler&& completion_handler,
                                  Implementation&& implementation)
        : Base(get_on_complete(allocation_type)),
          impl_(static_cast<CompletionHandler&&>(completion_handler), static_cast<Implementation&&>(implementation))
    {
    }

    CompletionHandler& completion_handler() noexcept { return impl_.first(); }

    void* tag() noexcept { return static_cast<Base*>(this); }

    detail::CompressedPair<CompletionHandler, Implementation> impl_;
};

template <class Initiation, class Implementation, class CompletionHandler>
auto initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation,
              SenderImplementationOperation<Implementation, CompletionHandler>& operation)
    -> decltype((void)initiation.initiate(grpc_context, std::declval<Implementation&>(), nullptr))
{
    initiation.initiate(grpc_context, operation.implementation(), operation.tag());
}

template <class Initiation, class Implementation, class CompletionHandler>
auto initiate(agrpc::GrpcContext& grpc_context, const Initiation& initiation,
              SenderImplementationOperation<Implementation, CompletionHandler>& operation)
    -> decltype((void)initiation.initiate(grpc_context, nullptr))
{
    initiation.initiate(grpc_context, operation.tag());
}

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
    auto operation = detail::allocate_operation<
        detail::SenderImplementationOperationTemplate<detail::RemoveCrefT<Implementation>>::template Type>(
        grpc_context, static_cast<CompletionHandler&&>(completion_handler),
        static_cast<Implementation&&>(implementation));
    grpc_context.work_started();
    operation->emplace_stop_callback(initiation);
    detail::initiate(grpc_context, initiation, *operation);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_IMPLEMENTATION_OPERATION_HPP
