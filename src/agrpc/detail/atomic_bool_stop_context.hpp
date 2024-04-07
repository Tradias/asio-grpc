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

#ifndef AGRPC_DETAIL_ATOMIC_BOOL_STOP_CONTEXT_HPP
#define AGRPC_DETAIL_ATOMIC_BOOL_STOP_CONTEXT_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>

#include <atomic>

#include <agrpc/detail/asio_macros.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class StopToken, bool = detail::IS_STOP_EVER_POSSIBLE_V<StopToken>>
class AtomicBoolStopContext;

template <class StopToken>
class AtomicBoolStopFunction
{
  public:
    explicit AtomicBoolStopFunction(AtomicBoolStopContext<StopToken>& context) noexcept : context_(context) {}

    void operator()() const noexcept { context_.stop(); }

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
    void operator()(asio::cancellation_type type) noexcept
    {
        if (static_cast<bool>(type & asio::cancellation_type::all))
        {
            operator()();
        }
    }
#endif

  private:
    AtomicBoolStopContext<StopToken>& context_;
};

template <class StopToken, bool>
class AtomicBoolStopContext : private detail::StopCallbackLifetime<StopToken, AtomicBoolStopFunction<StopToken>>
{
  private:
    using Base = detail::StopCallbackLifetime<StopToken, AtomicBoolStopFunction<StopToken>>;
    using StopFunction = AtomicBoolStopFunction<StopToken>;

  public:
    void emplace(StopToken&& stop_token) noexcept
    {
        Base::emplace(static_cast<StopToken&&>(stop_token), StopFunction{*this});
    }

    [[nodiscard]] bool is_stopped() const noexcept { return stopped_.load(std::memory_order_relaxed); }

    using Base::reset;

    void stop() noexcept
    {
        stopped_.store(true, std::memory_order_relaxed);
        reset();
    }

  private:
    std::atomic_bool stopped_{};
};

template <class StopToken>
class AtomicBoolStopContext<StopToken, false>
{
  public:
    static constexpr void emplace(const StopToken&) noexcept {}

    [[nodiscard]] static constexpr bool is_stopped() noexcept { return false; }

    static constexpr void reset() noexcept {}

    static constexpr void stop() noexcept {}
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ATOMIC_BOOL_STOP_CONTEXT_HPP
