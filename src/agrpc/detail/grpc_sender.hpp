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

#ifndef AGRPC_DETAIL_GRPC_SENDER_HPP
#define AGRPC_DETAIL_GRPC_SENDER_HPP

#include <agrpc/bind_allocator.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/grpc_context.hpp>
#include <agrpc/detail/grpc_submit.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/type_erased_operation.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver, class StopFunction>
inline constexpr bool GRPC_SENDER_HAS_STOP_CALLBACK =
    detail::IS_STOP_EVER_POSSIBLE_V<detail::exec::stop_token_type_t<Receiver&>>;

template <class Receiver>
inline constexpr bool GRPC_SENDER_HAS_STOP_CALLBACK<Receiver, detail::Empty> = false;

template <class InitiatingFunction, class StopFunction = detail::Empty>
class GrpcSender : public detail::SenderOf<bool>
{
  private:
    template <class Receiver>
    struct RunningOperation : detail::TypeErasedGrpcTagOperation
    {
        static constexpr bool HAS_STOP_CALLBACK = detail::GRPC_SENDER_HAS_STOP_CALLBACK<Receiver, StopFunction>;

        using StopCallbackLifetime =
            detail::ConditionalT<HAS_STOP_CALLBACK, std::optional<detail::StopCallbackTypeT<Receiver&, StopFunction>>,
                                 detail::Empty>;

        template <class Receiver2>
        explicit RunningOperation(Receiver2&& receiver)
            : detail::TypeErasedGrpcTagOperation(&on_complete), impl(std::forward<Receiver>(receiver))
        {
        }

        static void on_complete(detail::TypeErasedGrpcTagOperation* op, detail::InvokeHandler invoke_handler, bool ok,
                                detail::GrpcContextLocalAllocator) noexcept
        {
            auto& self = *static_cast<RunningOperation*>(op);
            if constexpr (HAS_STOP_CALLBACK)
            {
                self.stop_callback().reset();
            }
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler)
            {
                detail::satisfy_receiver(std::move(self.receiver()), ok);
            }
            else
            {
                detail::exec::set_done(std::move(self.receiver()));
            }
        }

        Receiver& receiver() noexcept { return impl.first(); }

        StopCallbackLifetime& stop_callback() noexcept { return impl.second(); }

        detail::CompressedPair<Receiver, StopCallbackLifetime> impl;
    };

    template <class Receiver>
    class Operation
    {
      private:
        static constexpr bool HAS_STOP_CALLBACK = detail::GRPC_SENDER_HAS_STOP_CALLBACK<Receiver, StopFunction>;

      public:
        template <class Receiver2>
        Operation(const GrpcSender& sender, Receiver2&& receiver)
            : grpc_context(sender.grpc_context),
              initiating_function(sender.initiating_function),
              running(std::forward<Receiver2>(receiver))
        {
        }

        void start() noexcept
        {
            if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context))
            {
                detail::exec::set_done(std::move(this->receiver()));
                return;
            }
            auto stop_token = detail::exec::get_stop_token(this->receiver());
            if (stop_token.stop_requested())
            {
                detail::exec::set_done(std::move(this->receiver()));
                return;
            }
            if constexpr (HAS_STOP_CALLBACK)
            {
                this->stop_callback().emplace(std::move(stop_token), StopFunction{initiating_function});
            }
            grpc_context.work_started();
            detail::WorkFinishedOnExit on_exit{grpc_context};
            initiating_function(grpc_context, &running);
            on_exit.release();
        }

      private:
        Receiver& receiver() noexcept { return running.receiver(); }

        auto& stop_callback() noexcept { return running.stop_callback(); }

        agrpc::GrpcContext& grpc_context;
        InitiatingFunction initiating_function;
        RunningOperation<Receiver> running;
    };

  public:
    template <class Receiver>
    auto connect(Receiver&& receiver) const noexcept(detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>)
        -> Operation<detail::RemoveCrefT<Receiver>>
    {
        return {*this, std::forward<Receiver>(receiver)};
    }

    template <class Receiver>
    void submit(Receiver&& receiver) const
    {
        if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(this->grpc_context))
        {
            detail::exec::set_done(std::forward<Receiver>(receiver));
            return;
        }
        auto allocator = detail::exec::get_allocator(receiver);
        detail::grpc_submit(this->grpc_context, this->initiating_function,
                            agrpc::AllocatorBinder(allocator,
                                                   [receiver = std::forward<Receiver>(receiver)](bool ok) mutable
                                                   {
                                                       detail::satisfy_receiver(std::move(receiver), ok);
                                                   }));
    }

  private:
    explicit GrpcSender(agrpc::GrpcContext& grpc_context, InitiatingFunction initiating_function) noexcept
        : grpc_context(grpc_context), initiating_function(std::move(initiating_function))
    {
    }

    friend detail::GrpcInitiateImplFn;

    agrpc::GrpcContext& grpc_context;
    InitiatingFunction initiating_function;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_SENDER_HPP
