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

#ifndef AGRPC_DETAIL_REPEATEDLY_REQUEST_BASE_HPP
#define AGRPC_DETAIL_REPEATEDLY_REQUEST_BASE_HPP

#include <agrpc/detail/asio_association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/query_grpc_context.hpp>
#include <agrpc/detail/rpc_type.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <bool IsStoppable>
class RepeatedlyRequestBaseStopContext
{
  public:
    explicit RepeatedlyRequestBaseStopContext(bool is_stoppable) noexcept : is_stoppable_(is_stoppable) {}

    [[nodiscard]] bool is_stopped() const noexcept
    {
        return is_stoppable_ ? stopped_.load(std::memory_order_relaxed) : false;
    }

    template <class StopFunction, class StopToken>
    void emplace(StopToken&& token)
    {
        using StopCallback = typename detail::RemoveCrefT<decltype(token)>::template callback_type<StopFunction>;
        [[maybe_unused]] StopCallback s{static_cast<StopToken&&>(token), StopFunction{stopped_}};
    }

  private:
    std::atomic_bool stopped_{};
    const bool is_stoppable_;
};

template <>
class RepeatedlyRequestBaseStopContext<false>
{
  public:
    constexpr explicit RepeatedlyRequestBaseStopContext(bool) noexcept {}

    [[nodiscard]] static constexpr bool is_stopped() noexcept { return false; }

    template <class StopFunction, class StopToken>
    static constexpr void emplace(StopToken&&) noexcept
    {
    }
};

template <class RequestHandler, class RPC, class CompletionHandler>
class RepeatedlyRequestOperationBase
{
  private:
    using Service = detail::GetServiceT<RPC>;
    using StopContext = detail::RepeatedlyRequestBaseStopContext<
        detail::IS_STOP_EVER_POSSIBLE_V<detail::exec::stop_token_type_t<CompletionHandler>>>;

  public:
    template <class Ch, class Rh>
    RepeatedlyRequestOperationBase(Rh&& request_handler, RPC rpc, Service& service, Ch&& completion_handler,
                                   bool is_stoppable)
        : impl1_(service, static_cast<Ch&&>(completion_handler)),
          impl2_(rpc, is_stoppable),
          request_handler_(static_cast<Rh&&>(request_handler))
    {
    }

    auto& cancellation_context() noexcept { return impl2_.second(); }

    auto& completion_handler() noexcept { return impl1_.second(); }

    decltype(auto) get_allocator() noexcept { return detail::exec::get_allocator(request_handler_); }

  protected:
    [[nodiscard]] bool is_stopped() const noexcept { return impl2_.second().is_stopped(); }

    decltype(auto) get_executor() noexcept { return detail::exec::get_executor(request_handler_); }

    agrpc::GrpcContext& grpc_context() noexcept { return detail::query_grpc_context(get_executor()); }

    RPC rpc() noexcept { return impl2_.first(); }

    Service& service() noexcept { return impl1_.first(); }

    RequestHandler& request_handler() noexcept { return request_handler_; }

  private:
    detail::CompressedPair<Service&, CompletionHandler> impl1_;
    detail::CompressedPair<RPC, StopContext> impl2_;
    RequestHandler request_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLY_REQUEST_BASE_HPP
