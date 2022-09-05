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

#include <agrpc/detail/asio_forward.hpp>
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
    explicit RepeatedlyRequestBaseStopContext(bool is_stoppable) noexcept : is_stoppable(is_stoppable) {}

    [[nodiscard]] bool is_stopped() const noexcept
    {
        return is_stoppable ? stopped.load(std::memory_order_relaxed) : false;
    }

    template <class StopFunction, class StopToken>
    void emplace(StopToken&& token)
    {
        using StopCallback = typename detail::RemoveCrefT<decltype(token)>::template callback_type<StopFunction>;
        [[maybe_unused]] StopCallback s{static_cast<StopToken&&>(token), StopFunction{stopped}};
    }

  private:
    std::atomic_bool stopped{};
    const bool is_stoppable;
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
        : impl1(service, static_cast<Ch&&>(completion_handler)),
          impl2(rpc, is_stoppable),
          request_handler_(static_cast<Rh&&>(request_handler))
    {
    }

    [[nodiscard]] auto& cancellation_context() noexcept { return impl2.second(); }

    [[nodiscard]] auto& completion_handler() noexcept { return impl1.second(); }

    [[nodiscard]] decltype(auto) get_allocator() noexcept
    {
        return detail::exec::get_allocator(this->request_handler());
    }

  protected:
    [[nodiscard]] bool is_stopped() const noexcept { return impl2.second().is_stopped(); }

    [[nodiscard]] decltype(auto) get_executor() noexcept { return detail::exec::get_executor(this->request_handler()); }

    [[nodiscard]] agrpc::GrpcContext& grpc_context() noexcept
    {
        return detail::query_grpc_context(this->get_executor());
    }

    [[nodiscard]] RPC rpc() noexcept { return impl2.first(); }

    [[nodiscard]] Service& service() noexcept { return impl1.first(); }

    [[nodiscard]] RequestHandler& request_handler() noexcept { return request_handler_; }

  private:
    detail::CompressedPair<Service&, CompletionHandler> impl1;
    detail::CompressedPair<RPC, StopContext> impl2;
    RequestHandler request_handler_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REPEATEDLY_REQUEST_BASE_HPP
