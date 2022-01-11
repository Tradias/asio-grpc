// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_REPEATEDLYREQUESTSENDER_HPP
#define AGRPC_DETAIL_REPEATEDLYREQUESTSENDER_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"
#include "agrpc/detail/receiver.hpp"
#include "agrpc/detail/rpcContext.hpp"
#include "agrpc/detail/senderOf.hpp"
#include "agrpc/detail/typeErasedOperation.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"
#include "agrpc/initiate.hpp"
#include "agrpc/rpcs.hpp"

#include <atomic>
#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Allocator>
struct NoOpReceiverWithAllocator
{
    using allocator_type = Allocator;

    Allocator allocator;

    constexpr explicit NoOpReceiverWithAllocator(Allocator allocator) noexcept(
        std::is_nothrow_copy_constructible_v<Allocator>)
        : allocator(allocator)
    {
    }

    static constexpr void set_done() noexcept
    {
        // no op
    }

    template <class... Args>
    static constexpr void set_value(Args&&...) noexcept
    {
        // no op
    }

    static void set_error(std::exception_ptr) noexcept
    {
        // no op
    }

    constexpr auto get_allocator() const noexcept { return allocator; }

#ifdef AGRPC_UNIFEX
    friend constexpr auto tag_invoke(unifex::tag_t<unifex::get_allocator>,
                                     const NoOpReceiverWithAllocator& receiver) noexcept
    {
        return receiver.allocator;
    }
#endif
};

struct RepeatedlyRequestStopFunction
{
    std::atomic<bool>& stopped;

    explicit RepeatedlyRequestStopFunction(std::atomic<bool>& stopped) noexcept : stopped(stopped) {}

    void operator()() const noexcept { stopped.store(true, std::memory_order_relaxed); }
};

template <class Receiver>
class RepeatedlyRequestStopContext
{
  public:
    template <class StopToken>
    void emplace(StopToken&& stop_token) noexcept
    {
        stop_callback.emplace(std::forward<StopToken>(stop_token),
                              detail::RepeatedlyRequestStopFunction{this->stopped});
    }

    void reset() noexcept { stop_callback.reset(); }

    [[nodiscard]] bool is_stopped() const noexcept { return stopped.load(std::memory_order_relaxed); }

  private:
    std::optional<detail::StopCallbackTypeT<Receiver&, detail::RepeatedlyRequestStopFunction>> stop_callback;
    std::atomic<bool> stopped{false};
};

template <class RPC, class Service, class SenderFactory>
class RepeatedlyRequestSender : public detail::SenderOf<>
{
  private:
    template <class Receiver>
    class Operation
    {
      private:
        static constexpr bool HAS_STOP_CALLBACK = detail::IS_STOP_EVER_POSSIBLE_V<detail::stop_token_type_t<Receiver&>>;

        struct DoneIfSender : detail::SenderOf<>
        {
            template <class IntermediateReceiver>
            struct IntermediateOperation
            {
                bool is_done;
                Operation& self;
                IntermediateReceiver intermediate_receiver;

                void start() & noexcept
                {
                    if (is_done)
                    {
                        detail::set_done(std::move(this->intermediate_receiver));
                        if constexpr (HAS_STOP_CALLBACK)
                        {
                            self.stop_context().reset();
                        }
                        detail::set_done(std::move(self.receiver()));
                        return;
                    }
                    detail::satisfy_receiver(std::move(this->intermediate_receiver));
                }
            };

            bool is_done;
            Operation& self;

            constexpr DoneIfSender(bool is_done, Operation& self) noexcept : is_done(is_done), self(self) {}

            template <class IntermediateReceiver>
            constexpr auto connect(IntermediateReceiver&& intermediate_receiver) const
                -> IntermediateOperation<detail::RemoveCvrefT<IntermediateReceiver>>
            {
                return {is_done, self, std::forward<IntermediateReceiver>(intermediate_receiver)};
            }
        };

      public:
        template <class Receiver2>
        constexpr Operation(const RepeatedlyRequestSender& sender, Receiver2&& receiver)
            : impl0(sender.grpc_context, std::forward<Receiver2>(receiver)),
              impl1(sender.rpc),
              impl2(sender.service, sender.sender_factory)
        {
        }

        template <class Receiver2>
        constexpr Operation(RepeatedlyRequestSender&& sender, Receiver2&& receiver)
            : impl0(sender.grpc_context, std::forward<Receiver2>(receiver)),
              impl1(sender.rpc),
              impl2(sender.service, std::move(sender.sender_factory))
        {
        }

        void start() & noexcept
        {
            if AGRPC_UNLIKELY (this->grpc_context().is_stopped())
            {
                detail::set_done(std::move(this->receiver()));
                return;
            }
            auto stop_token = detail::get_stop_token(this->receiver());
            if (stop_token.stop_requested())
            {
                detail::set_done(std::move(this->receiver()));
                return;
            }
            if constexpr (HAS_STOP_CALLBACK)
            {
                this->stop_context().emplace(std::move(stop_token));
            }
            this->submit_request_repeat();
        }

      private:
        void submit_request_repeat();

        constexpr bool is_stopped() noexcept
        {
            if constexpr (HAS_STOP_CALLBACK)
            {
                return this->stop_context().is_stopped();
            }
            else
            {
                return false;
            }
        }

        auto make_request(detail::RPCContextForRPCT<RPC>& context)
        {
            auto request_args = std::tuple_cat(std::forward_as_tuple(this->rpc(), this->service()), context.args(),
                                               std::tuple(agrpc::use_sender(this->grpc_context())));
            return std::apply(agrpc::request, request_args);
        }

        auto repeat()
        {
            return detail::on(this->grpc_context().get_scheduler(),
                              detail::let_value_with(
                                  []
                                  {
                                      return detail::RPCContextForRPCT<RPC>{};
                                  },
                                  [&](auto& context) mutable
                                  {
                                      return detail::let_value(
                                          detail::let_value(DoneIfSender{this->is_stopped(), *this},
                                                            [&]()
                                                            {
                                                                return this->make_request(context);
                                                            }),
                                          [&](bool ok) mutable
                                          {
                                              if AGRPC_LIKELY (ok)
                                              {
                                                  this->submit_request_repeat();
                                              }
                                              return detail::let_value(DoneIfSender{!ok, *this},
                                                                       [&]()
                                                                       {
                                                                           return std::apply(this->sender_factory(),
                                                                                             context.args());
                                                                       });
                                          });
                                  }));
        }

        constexpr decltype(auto) grpc_context() noexcept { return impl0.first(); }

        constexpr decltype(auto) receiver() noexcept { return impl0.second(); }

        constexpr decltype(auto) rpc() noexcept { return impl1.first(); }

        constexpr decltype(auto) stop_context() noexcept { return impl1.second(); }

        constexpr decltype(auto) service() noexcept { return impl2.first(); }

        constexpr decltype(auto) sender_factory() noexcept { return impl2.second(); }

        detail::CompressedPair<agrpc::GrpcContext&, Receiver> impl0;
        detail::CompressedPair<
            RPC, std::conditional_t<HAS_STOP_CALLBACK, detail::RepeatedlyRequestStopContext<Receiver>, detail::Empty>>
            impl1;
        detail::CompressedPair<Service&, SenderFactory> impl2;
    };

  public:
    template <class Receiver>
    constexpr auto connect(Receiver&& receiver) const& noexcept(
        std::is_nothrow_constructible_v<Receiver, Receiver&&>&& std::is_nothrow_copy_constructible_v<SenderFactory>)
        -> Operation<detail::RemoveCvrefT<Receiver>>
    {
        return {*this, std::forward<Receiver>(receiver)};
    }

    template <class Receiver>
    constexpr auto connect(Receiver&& receiver) && noexcept(
        std::is_nothrow_constructible_v<Receiver, Receiver&&>&& std::is_nothrow_move_constructible_v<SenderFactory>)
        -> Operation<detail::RemoveCvrefT<Receiver>>
    {
        return {std::move(*this), std::forward<Receiver>(receiver)};
    }

  private:
    constexpr explicit RepeatedlyRequestSender(
        agrpc::GrpcContext& grpc_context, RPC rpc, Service& service,
        SenderFactory sender_factory) noexcept(std::is_nothrow_move_constructible_v<SenderFactory>)
        : grpc_context(grpc_context), rpc(rpc), service(service), sender_factory(std::move(sender_factory))
    {
    }

    friend agrpc::detail::RepeatedlyRequestFn;

    agrpc::GrpcContext& grpc_context;
    RPC rpc;
    Service& service;
    SenderFactory sender_factory;
};

template <class RPC, class Service, class SenderFactory>
template <class Receiver>
void detail::RepeatedlyRequestSender<RPC, Service, SenderFactory>::Operation<Receiver>::submit_request_repeat()
{
    const auto allocator = detail::get_allocator(this->sender_factory());
    detail::submit(this->repeat(), detail::NoOpReceiverWithAllocator{allocator});
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUESTSENDER_HPP
