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

#ifndef AGRPC_DETAIL_SENDER_OPERATION_HPP
#define AGRPC_DETAIL_SENDER_OPERATION_HPP

#include <agrpc/detail/allocate.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/type_erased_operation.hpp>
#include <agrpc/detail/utility.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
enum class AllocationType
{
    NONE,
    LOCAL,
    REMOTE
};

template <AllocationType AllocType, class Operation>
auto extract_receiver_and_deallocate(Operation& operation, detail::GrpcContextLocalAllocator local_allocator) noexcept
{
    const auto& allocator = [&]
    {
        if constexpr (AllocType == AllocationType::LOCAL)
        {
            return local_allocator;
        }
        else
        {
            return detail::exec::get_allocator(operation.receiver());
        }
    }();
    auto receiver{std::move(operation.receiver())};
    detail::destroy_deallocate(&operation, allocator);
    return receiver;
}

template <AllocationType AllocType, class Operation>
auto extract_receiver_and_optionally_deallocate(Operation& operation,
                                                detail::GrpcContextLocalAllocator local_allocator) noexcept
{
    if constexpr (AllocType != AllocationType::NONE)
    {
        return detail::extract_receiver_and_deallocate<AllocType>(operation, local_allocator);
    }
    else
    {
        return std::move(operation.receiver());
    }
}

template <class Receiver, class StopFunction>
inline constexpr bool GRPC_SENDER_HAS_STOP_CALLBACK =
    detail::IS_STOP_EVER_POSSIBLE_V<detail::exec::stop_token_type_t<std::remove_reference_t<Receiver>&>>;

template <class Receiver>
inline constexpr bool GRPC_SENDER_HAS_STOP_CALLBACK<Receiver, detail::Empty> = false;

template <class DerivedTemplate, class Signature>
class SenderOperation;

template <class DerivedTemplate, class... Signature>
class SenderOperation<DerivedTemplate, void(Signature...)>
    : public detail::TypeErasedOperation<false, Signature..., detail::GrpcContextLocalAllocator>
{
  public:
    static constexpr AllocationType ALLOCATION_TYPE = DerivedTemplate::ALLOCATION_TYPE;

    using Receiver = typename DerivedTemplate::Receiver;
    using StopFunction = typename DerivedTemplate::StopFunction;

  private:
    using Impl = typename DerivedTemplate::Impl;

    static constexpr bool HAS_STOP_CALLBACK = detail::GRPC_SENDER_HAS_STOP_CALLBACK<Receiver, StopFunction>;

    using StopCallback =
        detail::ConditionalT<HAS_STOP_CALLBACK, std::optional<detail::StopCallbackTypeT<Receiver&, StopFunction>>,
                             detail::Empty>;

  public:
    template <class R>
    explicit SenderOperation(R&& receiver)
        : detail::TypeErasedOperation<false, Signature..., detail::GrpcContextLocalAllocator>(
              &SenderOperation::sender_operation_on_complete),
          impl(std::forward<R>(receiver))
    {
    }

    Receiver& receiver() noexcept { return impl.first(); }

    StopCallback& stop_callback() noexcept { return impl.second(); }

  private:
    struct Done
    {
        Impl& self;
        detail::InvokeHandler invoke_handler;
        detail::GrpcContextLocalAllocator local_allocator;

        template <class... Args>
        void operator()(Args&&... args)
        {
            if constexpr (HAS_STOP_CALLBACK)
            {
                self.stop_callback().reset();
            }
            auto receiver = detail::extract_receiver_and_optionally_deallocate<ALLOCATION_TYPE>(self, local_allocator);
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                detail::satisfy_receiver(std::move(receiver), std::forward<Args>(args)...);
            }
            else
            {
                detail::exec::set_done(std::move(receiver));
            }
        }
    };

    static void sender_operation_on_complete(detail::TypeErasedGrpcTagOperation* op,
                                             detail::InvokeHandler invoke_handler, Signature... args,
                                             detail::GrpcContextLocalAllocator local_allocator) noexcept
    {
        auto& self = *static_cast<Impl*>(op);
        self.on_complete(Done{self, invoke_handler, local_allocator}, static_cast<Signature&&>(args)...);
    }

    detail::CompressedPair<Receiver, StopCallback> impl;
};

template <class DerivedTemplate>
using GrpcTagSenderOperation = SenderOperation<DerivedTemplate, void(bool)>;

template <template <class, AllocationType> class Operation>
struct SenderOperationAllocationTraits
{
    template <class Receiver>
    using Local = Operation<Receiver, AllocationType::LOCAL>;

    template <class Receiver>
    using Remote = Operation<Receiver, AllocationType::REMOTE>;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_OPERATION_HPP
