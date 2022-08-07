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

#ifndef AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP
#define AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver, class StopFunction>
inline constexpr bool RECEIVER_NEEDS_STOP_CALLBACK =
    detail::IS_STOP_EVER_POSSIBLE_V<detail::exec::stop_token_type_t<std::remove_reference_t<Receiver>&>>;

template <class Receiver>
inline constexpr bool RECEIVER_NEEDS_STOP_CALLBACK<Receiver, detail::Empty> = false;

template <class Receiver, class StopFunction, bool = detail::RECEIVER_NEEDS_STOP_CALLBACK<Receiver, StopFunction>>
class ReceiverAndStopCallback
{
  private:
    using StopCallback = std::optional<detail::StopCallbackTypeT<Receiver&, StopFunction>>;

  public:
    template <class R>
    explicit ReceiverAndStopCallback(R&& receiver) : receiver_(std::forward<R>(receiver))
    {
    }

    Receiver& receiver() noexcept { return receiver_; }

    void reset_stop_callback() noexcept { stop_callback_.reset(); }

    template <class StopToken, class Implementation>
    void emplace_stop_callback(StopToken&& stop_token, Implementation& implementation) noexcept
    {
        stop_callback_.emplace(std::forward<StopToken>(stop_token), implementation.create_stop_function());
    }

  private:
    Receiver receiver_;
    StopCallback stop_callback_;
};

template <class Receiver, class StopFunction>
class ReceiverAndStopCallback<Receiver, StopFunction, false>
{
  public:
    template <class R>
    explicit ReceiverAndStopCallback(R&& receiver) : receiver_(std::forward<R>(receiver))
    {
    }

    Receiver& receiver() noexcept { return receiver_; }

    static constexpr void reset_stop_callback() noexcept {}

    template <class StopToken, class Implementation>
    static constexpr void emplace_stop_callback(StopToken&&, Implementation&) noexcept
    {
    }

  private:
    Receiver receiver_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP
