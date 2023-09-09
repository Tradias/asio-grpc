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

#ifndef AGRPC_DETAIL_REPEATEDLY_REQUEST_SENDER_HPP
#define AGRPC_DETAIL_REPEATEDLY_REQUEST_SENDER_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/atomic_bool_stop_context.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/operation_base.hpp>
#include <agrpc/detail/receiver.hpp>
#include <agrpc/detail/rpc_context.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <atomic>
#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class RPC, class RequestHandler>
class RepeatedlyRequestSender : public detail::SenderOf<void()>
{
  private:
    using Service = detail::GetServiceT<RPC>;

    template <class Receiver>
    class Operation : public detail::OperationBase
    {
      private:
        using Base = detail::OperationBase;
        using Allocator = detail::RemoveCrefT<decltype(detail::exec::get_allocator(std::declval<Receiver&>()))>;
        using RPCContext = detail::RPCContextForRPCT<RPC>;
        using RequestHandlerSender =
            detail::InvokeResultFromSignatureT<RequestHandler&, typename RPCContext::Signature>;

        static_assert(detail::exec::is_sender_v<RequestHandlerSender>,
                      "`repeatedly_request` request handler must return a sender.");

        struct RequestHandlerOperation
        {
            struct DeallocateRequestHandlerOperationReceiver
            {
                RequestHandlerOperation& repeat_operation_;

                explicit DeallocateRequestHandlerOperationReceiver(RequestHandlerOperation& repeat_operation) noexcept
                    : repeat_operation_(repeat_operation)
                {
                }

                void deallocate() noexcept
                {
                    detail::destroy_deallocate(&repeat_operation_, repeat_operation_.get_allocator());
                }

                void set_done() noexcept { deallocate(); }

                template <class... T>
                void set_value(T&&...) noexcept
                {
                    deallocate();
                }

                void set_error(const std::exception_ptr&) noexcept { deallocate(); }

                friend exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>,
                                                         const DeallocateRequestHandlerOperationReceiver&) noexcept
                {
                    return {};
                }
            };

            agrpc::GrpcContext& grpc_context_;
            RequestHandler request_handler_;
            detail::CompressedPair<Operation::RPCContext, Allocator> impl_;
            std::optional<detail::InplaceWithFunctionWrapper<
                detail::exec::connect_result_t<RequestHandlerSender, DeallocateRequestHandlerOperationReceiver>>>
                operation_state_;

            explicit RequestHandlerOperation(agrpc::GrpcContext& grpc_context, const RequestHandler& request_handler,
                                             const Allocator& allocator)
                : grpc_context_(grpc_context),
                  request_handler_(request_handler),
                  impl_(detail::SecondThenVariadic{}, allocator)
            {
            }

            void emplace_request_handler_operation()
            {
                operation_state_.emplace(detail::InplaceWithFunction{},
                                         [&]
                                         {
                                             return detail::exec::connect(
                                                 detail::invoke_from_rpc_context(request_handler_, rpc_context()),
                                                 DeallocateRequestHandlerOperationReceiver{*this});
                                         });
            }

            void start_request_handler_operation() { detail::exec::start(operation_state_->value_); }

            auto& rpc_context() noexcept { return impl_.first(); }

            auto& get_allocator() noexcept { return impl_.second(); }
        };

      public:
        void start() noexcept
        {
            if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context_))
            {
                detail::exec::set_done(static_cast<Receiver&&>(receiver_));
                return;
            }
            auto stop_token = detail::exec::get_stop_token(receiver_);
            if (detail::stop_requested(stop_token))
            {
                detail::exec::set_done(static_cast<Receiver&&>(receiver_));
                return;
            }
            stop_context().emplace(std::move(stop_token));
            initiate_repeatedly_request();
        }

      private:
        friend RepeatedlyRequestSender;

        template <class R>
        Operation(const RepeatedlyRequestSender& sender, R&& receiver)
            : Base(&Operation::do_request_complete),
              grpc_context_(sender.grpc_context_),
              receiver_(static_cast<R&&>(receiver)),
              impl_(sender.rpc_),
              service_(sender.service_),
              request_handler_(sender.request_handler_)
        {
        }

        template <class R>
        Operation(RepeatedlyRequestSender&& sender, R&& receiver)
            : Base(&Operation::do_request_complete),
              grpc_context_(sender.grpc_context_),
              receiver_(static_cast<R&&>(receiver)),
              impl_(sender.rpc_),
              service_(sender.service_),
              request_handler_(std::move(sender.request_handler_))
        {
        }

        bool is_stopped() noexcept { return stop_context().is_stopped(); }

        bool initiate_repeatedly_request()
        {
            auto& local_grpc_context = grpc_context_;
            if AGRPC_UNLIKELY (is_stopped())
            {
                return false;
            }
            const auto allocator = get_allocator();
            auto next_request_handler_operation =
                detail::allocate<RequestHandlerOperation>(allocator, grpc_context_, request_handler_, allocator);
            request_handler_operation_ = next_request_handler_operation.get();
            auto* cq = local_grpc_context.get_server_completion_queue();
            local_grpc_context.work_started();
            detail::initiate_request_from_rpc_context(rpc(), service_, request_handler_operation_->rpc_context(), cq,
                                                      this);
            next_request_handler_operation.release();
            return true;
        }

        static void do_request_complete(detail::OperationBase* op, detail::OperationResult result, agrpc::GrpcContext&)
        {
            auto* self = static_cast<Operation*>(op);
            detail::AllocationGuard ptr{self->request_handler_operation_, self->get_allocator()};
            if AGRPC_LIKELY (detail::OperationResult::OK == result)
            {
                if (auto exception_ptr = emplace_request_handler_operation(*ptr))
                {
                    self->stop_context().reset();
                    ptr.reset();
                    detail::exec::set_error(static_cast<Receiver&&>(self->receiver_), std::move(exception_ptr));
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
                if (detail::OperationResult::NOT_OK == result)  // server shutdown
                {
                    self->finish();
                }
                else
                {
                    self->done();
                }
            }
        }

        static std::exception_ptr emplace_request_handler_operation(RequestHandlerOperation& operation)
        {
            AGRPC_TRY
            {
                operation.emplace_request_handler_operation();
                return std::exception_ptr{};
            }
            AGRPC_CATCH(...) { return std::current_exception(); }
        }

        void finish()
        {
            stop_context().reset();
            detail::satisfy_receiver(static_cast<Receiver&&>(receiver_));
        }

        void done() noexcept
        {
            stop_context().reset();
            detail::exec::set_done(static_cast<Receiver&&>(receiver_));
        }

        RPC rpc() noexcept { return impl_.first(); }

        auto& stop_context() noexcept { return impl_.second(); }

        decltype(auto) get_allocator() noexcept { return detail::exec::get_allocator(receiver_); }

        agrpc::GrpcContext& grpc_context_;
        Receiver receiver_;
        detail::CompressedPair<RPC, detail::AtomicBoolStopContext<exec::stop_token_type_t<Receiver&>>> impl_;
        Service& service_;
        RequestHandlerOperation* request_handler_operation_;
        RequestHandler request_handler_;
    };

  public:
    template <class Receiver>
    auto connect(Receiver&& receiver) const& noexcept(
        detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_copy_constructible_v<RequestHandler>)
        -> Operation<detail::RemoveCrefT<Receiver>>
    {
        return {*this, static_cast<Receiver&&>(receiver)};
    }

    template <class Receiver>
    auto connect(Receiver&& receiver) && noexcept(
        detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_move_constructible_v<RequestHandler>)
        -> Operation<detail::RemoveCrefT<Receiver>>
    {
        return {static_cast<RepeatedlyRequestSender&&>(*this), static_cast<Receiver&&>(receiver)};
    }

  private:
    template <class Rh>
    RepeatedlyRequestSender(agrpc::GrpcContext& grpc_context, RPC rpc, Service& service, Rh&& request_handler)
        : grpc_context_(grpc_context),
          rpc_(rpc),
          service_(service),
          request_handler_(static_cast<Rh&&>(request_handler))
    {
    }

    friend detail::RepeatedlyRequestFn;

    agrpc::GrpcContext& grpc_context_;
    RPC rpc_;
    Service& service_;
    RequestHandler request_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLY_REQUEST_SENDER_HPP
