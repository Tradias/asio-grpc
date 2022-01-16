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
#include "agrpc/detail/noOpReceiverWithAllocator.hpp"
#include "agrpc/detail/receiver.hpp"
#include "agrpc/detail/rpcContext.hpp"
#include "agrpc/detail/senderOf.hpp"
#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"
#include "agrpc/rpcs.hpp"

#include <atomic>
#include <optional>
#include <variant>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
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

        struct RepeatSender : detail::SenderOf<>
        {
            template <class IntermediateReceiver>
            struct RepeatOperation
            {
                static auto make_request(Operation& self, detail::RPCContextForRPCT<RPC>& rpc_context) noexcept
                {
                    auto request_args =
                        std::tuple_cat(std::forward_as_tuple(self.rpc(), self.service()), rpc_context.args(),
                                       std::tuple(agrpc::use_sender(self.grpc_context())));
                    return std::apply(agrpc::request, request_args);
                }

                static auto make_request_handler(Operation& self, detail::RPCContextForRPCT<RPC>& rpc_context)
                {
                    return std::apply(self.sender_factory(), rpc_context.args());
                }

                void done() noexcept
                {
                    auto& local_self = this->self;
                    detail::set_done(std::move(this->intermediate_receiver));
                    if constexpr (HAS_STOP_CALLBACK)
                    {
                        local_self.stop_context().reset();
                    }
                    detail::set_done(std::move(local_self.receiver()));
                }

                void error(std::exception_ptr ep) noexcept
                {
                    auto& local_self = this->self;
                    detail::set_done(std::move(this->intermediate_receiver));
                    if constexpr (HAS_STOP_CALLBACK)
                    {
                        local_self.stop_context().reset();
                    }
                    detail::set_error(std::move(local_self.receiver()), std::move(ep));
                }

                template <bool Continue>
                struct AfterRequestReceiver
                {
                    RepeatOperation& repeat_operation;

                    explicit AfterRequestReceiver(RepeatOperation& repeat_operation) noexcept
                        : repeat_operation(repeat_operation)
                    {
                    }

                    static void handle_and_repeat_request(RepeatOperation& local_repeat_operation, bool ok)
                    {
                        if AGRPC_UNLIKELY (!ok)
                        {
                            local_repeat_operation.done();
                            return;
                        }
                        local_repeat_operation.operation_state.template emplace<2>(
                            detail::InplaceWithFunction{},
                            [&]
                            {
                                return detail::connect(make_request_handler(local_repeat_operation.self,
                                                                            local_repeat_operation.rpc_context),
                                                       AfterRequestReceiver<false>{local_repeat_operation});
                            });
                        local_repeat_operation.self.submit_request_repeat();
                        detail::start(std::get<2>(local_repeat_operation.operation_state).value);
                    }

                    void set_done() noexcept { this->repeat_operation.done(); }

                    template <class... Args>
                    void set_value([[maybe_unused]] Args&&... args)
                    {
                        if constexpr (Continue)
                        {
                            handle_and_repeat_request(this->repeat_operation, args...);
                        }
                        else
                        {
                            detail::satisfy_receiver(std::move(this->repeat_operation.intermediate_receiver));
                        }
                    }

                    void set_error(std::exception_ptr ep) noexcept { this->repeat_operation.error(std::move(ep)); }
                };

                Operation& self;
                IntermediateReceiver intermediate_receiver;
                detail::RPCContextForRPCT<RPC> rpc_context;
                std::variant<std::monostate,
                             detail::InplaceWithFunctionWrapper<detail::connect_result_t<
                                 decltype(make_request(self, rpc_context)), AfterRequestReceiver<true>>>,
                             detail::InplaceWithFunctionWrapper<detail::connect_result_t<
                                 decltype(make_request_handler(self, rpc_context)), AfterRequestReceiver<false>>>>
                    operation_state;

                template <class Receiver2>
                RepeatOperation(Operation& self, Receiver2&& receiver) noexcept(
                    std::is_nothrow_constructible_v<IntermediateReceiver, Receiver2&&>)
                    : self(self), intermediate_receiver(std::forward<Receiver2>(receiver))
                {
                }

                void start() & noexcept
                {
                    if (self.is_stopped())
                    {
                        this->done();
                        return;
                    }
                    this->operation_state.template emplace<1>(detail::InplaceWithFunction{},
                                                              [&]
                                                              {
                                                                  return detail::connect(
                                                                      make_request(this->self, this->rpc_context),
                                                                      AfterRequestReceiver<true>{*this});
                                                              });
                    detail::start(std::get<1>(this->operation_state).value);
                }
            };

            explicit RepeatSender(Operation& self) noexcept : self(self) {}

            template <class IntermediateReceiver>
            auto connect(IntermediateReceiver&& receiver) const noexcept
                -> RepeatOperation<detail::RemoveCvrefT<IntermediateReceiver>>
            {
                return {self, std::forward<IntermediateReceiver>(receiver)};
            }

            Operation& self;
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
        bool is_stopped() noexcept
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

        void submit_request_repeat()
        {
            const auto allocator = detail::get_allocator(this->sender_factory());
            detail::submit(RepeatSender{*this}, detail::NoOpReceiverWithAllocator{allocator});
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
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUESTSENDER_HPP
