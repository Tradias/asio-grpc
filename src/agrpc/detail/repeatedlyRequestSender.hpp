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

#include <atomic>
#include <optional>
#include <variant>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver, bool = detail::IS_STOP_EVER_POSSIBLE_V<detail::exec::stop_token_type_t<Receiver&>>>
class RepeatedlyRequestStopContext
{
  private:
    class StopFunction
    {
      public:
        explicit StopFunction(RepeatedlyRequestStopContext& context) noexcept : context(context) {}

        void operator()() const noexcept { this->context.stop(); }

      private:
        RepeatedlyRequestStopContext& context;
    };

  public:
    template <class StopToken>
    void emplace(StopToken&& stop_token) noexcept
    {
        this->stop_callback.emplace(std::forward<StopToken>(stop_token), StopFunction{*this});
    }

    [[nodiscard]] bool is_stopped() const noexcept { return this->stopped.load(std::memory_order_relaxed); }

    void reset() noexcept { this->stop_callback.reset(); }

  private:
    void stop() noexcept
    {
        this->stopped.store(true, std::memory_order_relaxed);
        this->reset();
    }

    std::optional<detail::StopCallbackTypeT<Receiver&, StopFunction>> stop_callback;
    std::atomic<bool> stopped;
};

template <class Receiver>
class RepeatedlyRequestStopContext<Receiver, false>
{
  public:
    template <class StopToken>
    static constexpr void emplace(StopToken&&) noexcept
    {
    }

    [[nodiscard]] static constexpr bool is_stopped() noexcept { return false; }

    static constexpr void reset() noexcept {}
};

template <class RPC, class Service, class RequestHandler>
class RepeatedlyRequestSender : public detail::SenderOf<>
{
  private:
    template <class Receiver>
    class Operation : public detail::TypeErasedGrpcTagOperation
    {
      private:
        using GrpcBase = detail::TypeErasedGrpcTagOperation;
        using Allocator = detail::RemoveCvrefT<decltype(detail::exec::get_allocator(std::declval<Receiver&>()))>;
        using RPCContext = detail::RPCContextForRPCT<RPC>;
        using RequestHandlerSender =
            detail::InvokeResultFromSignatureT<RequestHandler&, typename RPCContext::Signature>;

        static_assert(detail::exec::is_sender_v<RequestHandlerSender>,
                      "`repeatedly_request` request handler must return a sender.");

        struct RequestHandlerOperation
        {
            struct DeallocateRequestHandlerOperationReceiver
            {
                RequestHandlerOperation& repeat_operation;

                explicit DeallocateRequestHandlerOperationReceiver(RequestHandlerOperation& repeat_operation) noexcept
                    : repeat_operation(repeat_operation)
                {
                }

                void deallocate() noexcept
                {
                    auto& local_grpc_context = repeat_operation.grpc_context;
                    detail::AllocatedPointer{&repeat_operation, repeat_operation.get_allocator()};
                    local_grpc_context.work_finished();
                }

                void set_done() noexcept { deallocate(); }

                template <class... T>
                void set_value(T&&...) noexcept
                {
                    deallocate();
                }

                void set_error(std::exception_ptr) noexcept { deallocate(); }
            };

            agrpc::GrpcContext& grpc_context;
            detail::CompressedPair<detail::RPCContextForRPCT<RPC>, Allocator> impl;
            std::optional<detail::InplaceWithFunctionWrapper<
                detail::exec::connect_result_t<RequestHandlerSender, DeallocateRequestHandlerOperationReceiver>>>
                operation_state;

            explicit RequestHandlerOperation(agrpc::GrpcContext& grpc_context, const Allocator& allocator)
                : grpc_context(grpc_context), impl(detail::SecondThenVariadic{}, allocator)
            {
            }

            void emplace_request_handler_operation(RequestHandler& request_handler)
            {
                operation_state.emplace(detail::InplaceWithFunction{},
                                        [&]
                                        {
                                            return detail::exec::connect(
                                                std::apply(request_handler, rpc_context().args()),
                                                DeallocateRequestHandlerOperationReceiver{*this});
                                        });
            }

            void start_request_handler_operation()
            {
                grpc_context.work_started();
                detail::exec::start(operation_state->value);
            }

            auto& rpc_context() noexcept { return impl.first(); }

            auto& get_allocator() noexcept { return impl.second(); }
        };

      public:
        void start() noexcept
        {
            if AGRPC_UNLIKELY (this->grpc_context().is_stopped())
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
            this->stop_context().emplace(std::move(stop_token));
            this->initiate_repeatedly_request();
        }

      private:
        friend RepeatedlyRequestSender;

        template <class Receiver2>
        Operation(const RepeatedlyRequestSender& sender, Receiver2&& receiver)
            : GrpcBase(&Operation::on_request_complete),
              impl0(sender.grpc_context, std::forward<Receiver2>(receiver)),
              impl1(sender.rpc),
              impl2(sender.impl)
        {
        }

        template <class Receiver2>
        Operation(RepeatedlyRequestSender&& sender, Receiver2&& receiver)
            : GrpcBase(&Operation::on_request_complete),
              impl0(sender.grpc_context, std::forward<Receiver2>(receiver)),
              impl1(sender.rpc),
              impl2(std::move(sender.impl))
        {
        }

        bool is_stopped() noexcept { return this->stop_context().is_stopped(); }

        auto allocate_request_handler_operation()
        {
            const auto allocator = this->get_allocator();
            auto ptr = detail::allocate<RequestHandlerOperation>(allocator, this->grpc_context(), allocator);
            this->request_handler_operation = ptr.get();
            return ptr;
        }

        bool initiate_repeatedly_request()
        {
            auto& local_grpc_context = this->grpc_context();
            if AGRPC_UNLIKELY (this->is_stopped() || local_grpc_context.is_stopped())
            {
                return false;
            }
            auto ptr = this->allocate_request_handler_operation();
            auto* cq = local_grpc_context.get_server_completion_queue();
            local_grpc_context.work_started();
            detail::initiate_request_from_rpc_context(this->rpc(), this->service(), ptr->rpc_context(), cq, this);
            ptr.release();
            return true;
        }

        static void on_request_complete(GrpcBase* op, detail::InvokeHandler invoke_handler, bool ok,
                                        detail::GrpcContextLocalAllocator)
        {
            auto* self = static_cast<Operation*>(op);
            detail::AllocatedPointer ptr{self->request_handler_operation, self->get_allocator()};
            if AGRPC_LIKELY (detail::InvokeHandler::YES == invoke_handler && ok)
            {
                if (auto ep = self->emplace_request_handler_operation(*ptr); ep)
                {
                    self->stop_context().reset();
                    ptr.reset();
                    detail::exec::set_error(std::move(self->receiver()), std::move(ep));
                    return;
                }
                const auto is_repeated = self->initiate_repeatedly_request();
                ptr->start_request_handler_operation();
                ptr.release();
                if (!is_repeated)
                {
                    self->done();
                }
            }
            else
            {
                ptr.reset();
                if (detail::InvokeHandler::YES == invoke_handler && !ok)  // server shutdown
                {
                    self->finish();
                }
                else
                {
                    self->done();
                }
            }
        }

        auto emplace_request_handler_operation(RequestHandlerOperation& operation)
        {
            AGRPC_TRY
            {
                operation.emplace_request_handler_operation(this->request_handler());
                return std::exception_ptr{};
            }
            AGRPC_CATCH(...) { return std::current_exception(); }
        }

        void finish()
        {
            this->stop_context().reset();
            detail::satisfy_receiver(std::move(receiver()));
        }

        void done() noexcept
        {
            this->stop_context().reset();
            detail::exec::set_done(std::move(receiver()));
        }

        constexpr auto& grpc_context() noexcept { return impl0.first(); }

        constexpr auto& receiver() noexcept { return impl0.second(); }

        constexpr auto& rpc() noexcept { return impl1.first(); }

        constexpr auto& stop_context() noexcept { return impl1.second(); }

        constexpr auto& service() noexcept { return impl2.first(); }

        constexpr auto& request_handler() noexcept { return impl2.second(); }

        constexpr decltype(auto) get_allocator() noexcept { return detail::exec::get_allocator(this->receiver()); }

        detail::CompressedPair<agrpc::GrpcContext&, Receiver> impl0;
        detail::CompressedPair<RPC, detail::RepeatedlyRequestStopContext<Receiver>> impl1;
        detail::CompressedPair<Service&, RequestHandler> impl2;
        RequestHandlerOperation* request_handler_operation;
    };

  public:
    template <class Receiver>
    auto connect(Receiver&& receiver) const& noexcept(
        std::is_nothrow_constructible_v<Receiver, Receiver&&>&& std::is_nothrow_copy_constructible_v<RequestHandler>)
        -> Operation<detail::RemoveCvrefT<Receiver>>
    {
        return {*this, std::forward<Receiver>(receiver)};
    }

    template <class Receiver>
    auto connect(Receiver&& receiver) && noexcept(
        std::is_nothrow_constructible_v<Receiver, Receiver&&>&& std::is_nothrow_move_constructible_v<RequestHandler>)
        -> Operation<detail::RemoveCvrefT<Receiver>>
    {
        return {std::move(*this), std::forward<Receiver>(receiver)};
    }

  private:
    template <class Rh>
    RepeatedlyRequestSender(agrpc::GrpcContext& grpc_context, RPC rpc, Service& service, Rh&& request_handler)
        : grpc_context(grpc_context), rpc(rpc), impl(service, std::forward<Rh>(request_handler))
    {
    }

    friend agrpc::detail::RepeatedlyRequestFn;

    agrpc::GrpcContext& grpc_context;
    RPC rpc;
    detail::CompressedPair<Service&, RequestHandler> impl;
};

template <class RPC, class Service, class RequestHandler>
RepeatedlyRequestSender(agrpc::GrpcContext&, RPC, Service&, RequestHandler&&)
    -> RepeatedlyRequestSender<RPC, Service, detail::RemoveCvrefT<RequestHandler>>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLYREQUESTSENDER_HPP
