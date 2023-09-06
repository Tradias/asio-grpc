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

#ifndef AGRPC_DETAIL_REQUEST_HANDLER_SENDER_HPP
#define AGRPC_DETAIL_REQUEST_HANDLER_SENDER_HPP

#include <agrpc/detail/asio_association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/no_op_stop_callback.hpp>
#include <agrpc/detail/rpc_request.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <atomic>
#include <optional>
#include <variant>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver, bool = detail::IS_STOP_EVER_POSSIBLE_V<exec::stop_token_type_t<Receiver&>>>
class RequestHandlerSenderStopContext
{
  private:
    class StopFunction
    {
      public:
        explicit StopFunction(RequestHandlerSenderStopContext& context) noexcept : context_(context) {}

        void operator()() const noexcept { context_.stop(); }

      private:
        RequestHandlerSenderStopContext& context_;
    };

  public:
    template <class StopToken>
    void emplace(StopToken&& stop_token) noexcept
    {
        stop_callback_.emplace(static_cast<StopToken&&>(stop_token), StopFunction{*this});
    }

    [[nodiscard]] bool is_stopped() const noexcept { return stopped_.load(std::memory_order_relaxed); }

    void reset() noexcept { stop_callback_.reset(); }

  private:
    void stop() noexcept
    {
        stopped_.store(true, std::memory_order_relaxed);
        reset();
    }

    std::optional<detail::StopCallbackTypeT<Receiver&, StopFunction>> stop_callback_;
    std::atomic<bool> stopped_;
};

template <class Receiver>
class RequestHandlerSenderStopContext<Receiver, false> : public detail::NoOpStopCallback
{
};

template <class ServerRPC>
struct GetServerRPCService;

template <class Service, class Request, class Responder,
          detail::ServerMultiArgRequest<Service, Request, Responder> RequestRPC, class Traits, class Executor>
struct GetServerRPCService<agrpc::ServerRPC<RequestRPC, Traits, Executor>>
{
    using Type = Service;
};

template <class Service, class Responder, detail::ServerSingleArgRequest<Service, Responder> RequestRPC, class Traits,
          class Executor>
struct GetServerRPCService<agrpc::ServerRPC<RequestRPC, Traits, Executor>>
{
    using Type = Service;
};

template <class Traits, class Executor>
struct GetServerRPCService<agrpc::GenericServerRPC<Traits, Executor>>
{
    using Type = grpc::AsyncGenericService;
};

template <class RPC>
using GetServerRPCServiceT = typename detail::GetServerRPCService<RPC>::Type;

template <class Operation>
struct DeallocateOperationReceiver
{
    Operation& op_;

    explicit DeallocateOperationReceiver(Operation& op) noexcept : op_(op) {}

    void deallocate() noexcept { detail::destroy_deallocate(&op_, op_.get_allocator()); }

    void set_done() noexcept { deallocate(); }

    template <class... T>
    void set_value(T&&...) noexcept
    {
        deallocate();
    }

    void set_error(const std::exception_ptr&) noexcept { deallocate(); }

    friend exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>,
                                             const DeallocateOperationReceiver&) noexcept
    {
        return {};
    }
};

template <class ServerRPC, class RequestHandler>
class RequestHandlerSender : public detail::SenderOf<void()>
{
  private:
    using Service = detail::GetServerRPCServiceT<ServerRPC>;

    template <class Receiver>
    class Operation
    {
      private:
        using Allocator = detail::RemoveCrefT<decltype(exec::get_allocator(std::declval<Receiver&>()))>;
        using InitialRequest =
            detail::RPCRequest<typename ServerRPC::Request, detail::has_initial_request(ServerRPC::TYPE)>;
        using RequestHandlerInvokeResult =
            decltype(std::declval<InitialRequest>().invoke(std::declval<RequestHandler>(), std::declval<ServerRPC&>()));

        static_assert(exec::is_sender_v<RequestHandlerInvokeResult>, "Request handler must return a sender.");

        struct RequestHandlerOperation
        {
            using DeallocateRequestHandlerOperationReceiver =
                detail::DeallocateOperationReceiver<RequestHandlerOperation>;
            using RequestHandlerOperationState = detail::InplaceWithFunctionWrapper<
                exec::connect_result_t<RequestHandlerInvokeResult, DeallocateRequestHandlerOperationReceiver>>;

            struct StartReceiver
            {
                Operation& op_;
                RequestHandlerOperation& request_handler_op_;

                StartReceiver(Operation& op, RequestHandlerOperation& request_handler_op) noexcept
                    : op_(op), request_handler_op_(request_handler_op)
                {
                }

                [[noreturn]] void set_done() noexcept { std::terminate(); }

                void set_value(bool ok)
                {
                    detail::AllocationGuard ptr{&request_handler_op_, request_handler_op_.get_allocator()};
                    if (ok)
                    {
                        if (auto exception_ptr = request_handler_op_.emplace_request_handler_operation_state())
                        {
                            ptr.reset();
                            op_.set_error(std::move(exception_ptr));
                            return;
                        }
                        const auto is_repeated = op_.initiate_repeatedly_request();
                        request_handler_op_.start_request_handler_operation_state();
                        ptr.release();
                        if (!is_repeated)
                        {
                            op_.set_done();
                        }
                    }
                    else
                    {
                        ptr.reset();
                        op_.set_done();
                    }
                }

                [[noreturn]] void set_error(const std::exception_ptr&) noexcept { std::terminate(); }

                friend exec::inline_scheduler tag_invoke(exec::tag_t<exec::get_scheduler>,
                                                         const StartReceiver&) noexcept
                {
                    return {};
                }
            };

            using StartOperationState = detail::InplaceWithFunctionWrapper<
                exec::connect_result_t<decltype(std::declval<InitialRequest>().start(
                                           std::declval<ServerRPC&>(), std::declval<Service&>(), agrpc::use_sender)),
                                       StartReceiver>>;

            using OperationState = std::variant<StartOperationState, RequestHandlerOperationState>;

            explicit RequestHandlerOperation(Operation& operation)
                : impl1_(operation.request_handler_),
                  rpc_(
                      detail::ServerRPCContextBaseAccess::construct<ServerRPC>(operation.grpc_context_.get_executor())),
                  impl2_(detail::SecondThenVariadic{}, operation.get_allocator(),
                         std::in_place_type<StartOperationState>, detail::InplaceWithFunction{},
                         [&]
                         {
                             return exec::connect(initial_request().start(rpc_, operation.service_, agrpc::use_sender),
                                                  StartReceiver{operation, *this});
                         })
            {
            }

            void start() { exec::start(std::get<StartOperationState>(operation_state()).value_); }

            std::exception_ptr emplace_request_handler_operation_state()
            {
                AGRPC_TRY
                {
                    operation_state().template emplace<RequestHandlerOperationState>(
                        detail::InplaceWithFunction{},
                        [&]
                        {
                            return exec::connect(
                                initial_request().invoke(static_cast<RequestHandler&&>(request_handler()), rpc_),
                                DeallocateRequestHandlerOperationReceiver{*this});
                        });
                    return std::exception_ptr{};
                }
                AGRPC_CATCH(...) { return std::current_exception(); }
            }

            void start_request_handler_operation_state()
            {
                exec::start(std::get<RequestHandlerOperationState>(operation_state()).value_);
            }

            auto& request_handler() noexcept { return impl1_.first(); }

            auto& initial_request() noexcept { return impl1_.second(); }

            auto& operation_state() noexcept { return impl2_.first(); }

            auto& get_allocator() noexcept { return impl2_.second(); }

            detail::CompressedPair<RequestHandler, InitialRequest> impl1_;
            ServerRPC rpc_;
            detail::CompressedPair<OperationState, Allocator> impl2_;
        };

      public:
        void start() noexcept
        {
            if AGRPC_UNLIKELY (detail::GrpcContextImplementation::is_shutdown(grpc_context_))
            {
                exec::set_done(static_cast<Receiver&&>(receiver_));
                return;
            }
            auto stop_token = exec::get_stop_token(receiver_);
            if (stop_token.stop_requested())
            {
                exec::set_done(static_cast<Receiver&&>(receiver_));
                return;
            }
            stop_context_.emplace(std::move(stop_token));
            initiate_repeatedly_request();
        }

      private:
        friend RequestHandlerSender;

        template <class R>
        Operation(RequestHandlerSender&& sender, R&& receiver)
            : grpc_context_(sender.grpc_context_),
              receiver_(static_cast<R&&>(receiver)),
              service_(sender.service_),
              request_handler_(std::move(sender.request_handler_))
        {
        }

        bool is_stopped() const noexcept { return stop_context_.is_stopped(); }

        bool initiate_repeatedly_request()
        {
            if AGRPC_UNLIKELY (is_stopped())
            {
                return false;
            }
            auto request_handler_operation = detail::allocate<RequestHandlerOperation>(get_allocator(), *this);
            request_handler_operation->start();
            request_handler_operation.release();
            return true;
        }

        void set_done() noexcept
        {
            stop_context_.reset();
            exec::set_done(static_cast<Receiver&&>(receiver_));
        }

        template <class Error>
        void set_error(Error&& error)
        {
            stop_context_.reset();
            exec::set_error(static_cast<Receiver&&>(receiver_), static_cast<Error&&>(error));
        }

        decltype(auto) get_allocator() noexcept { return exec::get_allocator(receiver_); }

        agrpc::GrpcContext& grpc_context_;
        Receiver receiver_;
        detail::RequestHandlerSenderStopContext<Receiver> stop_context_;
        Service& service_;
        RequestHandler request_handler_;
    };

  public:
    RequestHandlerSender(agrpc::GrpcContext& grpc_context, Service& service, RequestHandler&& request_handler)
        : grpc_context_(grpc_context),
          service_(service),
          request_handler_(static_cast<RequestHandler&&>(request_handler))
    {
    }

    template <class Receiver>
    auto connect(Receiver&& receiver) && noexcept(
        detail::IS_NOTRHOW_DECAY_CONSTRUCTIBLE_V<Receiver>&& std::is_nothrow_move_constructible_v<RequestHandler>)
        -> Operation<detail::RemoveCrefT<Receiver>>
    {
        return {static_cast<RequestHandlerSender&&>(*this), static_cast<Receiver&&>(receiver)};
    }

  private:
    agrpc::GrpcContext& grpc_context_;
    Service& service_;
    RequestHandler request_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REQUEST_HANDLER_SENDER_HPP
