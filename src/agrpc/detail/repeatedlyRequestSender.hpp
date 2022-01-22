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
class RepeatedlyRequestStopFunction
{
  public:
    explicit RepeatedlyRequestStopFunction(std::atomic<bool>& stopped) noexcept : stopped(stopped) {}

    void operator()() const noexcept { stopped.store(true, std::memory_order_relaxed); }

  private:
    std::atomic<bool>& stopped;
};

template <class Receiver>
class RepeatedlyRequestStopContext
{
  private:
    static constexpr bool HAS_STOP_CALLBACK = detail::IS_STOP_EVER_POSSIBLE_V<detail::stop_token_type_t<Receiver&>>;

    using StopCallbackLifetime =
        std::conditional_t<HAS_STOP_CALLBACK,
                           std::optional<detail::StopCallbackTypeT<Receiver&, detail::RepeatedlyRequestStopFunction>>,
                           detail::Empty>;

  public:
    template <class StopToken>
    void emplace([[maybe_unused]] StopToken&& stop_token) noexcept
    {
        if constexpr (HAS_STOP_CALLBACK)
        {
            this->stop_callback().emplace(std::forward<StopToken>(stop_token),
                                          detail::RepeatedlyRequestStopFunction{this->stopped()});
        }
    }

    void reset() noexcept
    {
        if constexpr (HAS_STOP_CALLBACK)
        {
            this->stop_callback().reset();
        }
    }

    void stop() noexcept { this->stopped().store(true, std::memory_order_relaxed); }

    [[nodiscard]] bool is_stopped() const noexcept { return this->stopped().load(std::memory_order_relaxed); }

    void work_started() noexcept { this->outstanding_work_.fetch_add(1, std::memory_order_relaxed); }

    auto work_finished() noexcept { return this->outstanding_work_.fetch_sub(1, std::memory_order_relaxed); }

    [[nodiscard]] auto outstanding_work() const noexcept
    {
        return this->outstanding_work_.load(std::memory_order_relaxed);
    }

  private:
    decltype(auto) stopped() noexcept { return impl.first(); }

    decltype(auto) stopped() const noexcept { return impl.first(); }

    decltype(auto) stop_callback() noexcept { return impl.second(); }

    detail::CompressedPair<std::atomic<bool>, StopCallbackLifetime> impl;
    std::atomic_size_t outstanding_work_{};
};

template <class RPC, class Service, class SenderFactory>
class RepeatedlyRequestSender : public detail::SenderOf<>
{
  private:
    template <class Receiver>
    class Operation
    {
      private:
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
                    static_assert(detail::is_sender_v<decltype(std::apply(self.sender_factory(), rpc_context.args()))>,
                                  "`repeatedly_request` handler factory must return a sender.");
                    return std::apply(self.sender_factory(), rpc_context.args());
                }

                template <class CPO, class... Args>
                static void finally(Operation& local_self, CPO&& cpo, Args&&... args)
                {
                    const auto work_remaining = local_self.stop_context().work_finished();
                    if (local_self.is_stopped() && 1 == work_remaining)
                    {
                        local_self.stop_context().reset();
                        std::forward<CPO>(cpo)(std::move(local_self.receiver()), std::forward<Args>(args)...);
                    }
                }

                void done() noexcept
                {
                    auto& local_self = this->self;
                    detail::set_done(std::move(this->intermediate_receiver));
                    finally(local_self, detail::set_done);
                }

                void fulfill()
                {
                    auto& local_self = this->self;
                    detail::set_value(std::move(this->intermediate_receiver));
                    finally(local_self, detail::set_value);
                }

                void error(std::exception_ptr ep) noexcept
                {
                    auto& local_self = this->self;
                    detail::set_done(std::move(this->intermediate_receiver));
                    local_self.stop_context().stop();
                    finally(local_self, detail::set_error, std::move(ep));
                }

                struct AfterRequestHandledReceiver
                {
                    RepeatOperation& repeat_operation;

                    explicit AfterRequestHandledReceiver(RepeatOperation& repeat_operation) noexcept
                        : repeat_operation(repeat_operation)
                    {
                    }

                    void set_done() noexcept { this->repeat_operation.done(); }

                    template <class... Args>
                    void set_value(Args&&...)
                    {
                        this->repeat_operation.fulfill();
                    }

                    void set_error(std::exception_ptr ep) noexcept { this->repeat_operation.error(std::move(ep)); }
                };

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
                            local_repeat_operation.self.stop_context().stop();
                            if (1 == local_repeat_operation.self.stop_context().outstanding_work())
                            {
                                local_repeat_operation.fulfill();
                            }
                            return;
                        }
                        local_repeat_operation.operation_state.template emplace<2>(
                            detail::InplaceWithFunction{},
                            [&]
                            {
                                return detail::connect(make_request_handler(local_repeat_operation.self,
                                                                            local_repeat_operation.rpc_context),
                                                       AfterRequestHandledReceiver{local_repeat_operation});
                            });
                        local_repeat_operation.self.submit_request_repeat();
                        detail::start(std::get<2>(local_repeat_operation.operation_state).value);
                    }

                    void set_done() noexcept { this->repeat_operation.done(); }

                    void set_value(bool ok) { handle_and_repeat_request(this->repeat_operation, ok); }

                    void set_error(std::exception_ptr ep) noexcept { this->repeat_operation.error(std::move(ep)); }
                };

                Operation& self;
                IntermediateReceiver intermediate_receiver;
                detail::RPCContextForRPCT<RPC> rpc_context;
                std::variant<std::monostate,
                             detail::InplaceWithFunctionWrapper<detail::connect_result_t<
                                 decltype(make_request(self, rpc_context)), AfterRequestReceiver>>,
                             detail::InplaceWithFunctionWrapper<detail::connect_result_t<
                                 decltype(make_request_handler(self, rpc_context)), AfterRequestHandledReceiver>>>
                    operation_state;

                template <class Receiver2>
                RepeatOperation(Operation& self, Receiver2&& receiver) noexcept(
                    std::is_nothrow_constructible_v<IntermediateReceiver, Receiver2&&>)
                    : self(self), intermediate_receiver(std::forward<Receiver2>(receiver))
                {
                }

                void start() & noexcept
                {
                    if AGRPC_UNLIKELY (self.is_stopped() && 1 == self.stop_context().outstanding_work())
                    {
                        finally(self, detail::set_done);
                        return;
                    }
                    this->operation_state.template emplace<1>(detail::InplaceWithFunction{},
                                                              [&]
                                                              {
                                                                  return detail::connect(
                                                                      make_request(this->self, this->rpc_context),
                                                                      AfterRequestReceiver{*this});
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
            this->stop_context().emplace(std::move(stop_token));
            this->submit_request_repeat();
        }

      private:
        bool is_stopped() noexcept { return this->stop_context().is_stopped(); }

        void submit_request_repeat()
        {
            if AGRPC_UNLIKELY (this->is_stopped())
            {
                return;
            }
            this->stop_context().work_started();
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
        detail::CompressedPair<RPC, detail::RepeatedlyRequestStopContext<Receiver>> impl1;
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
