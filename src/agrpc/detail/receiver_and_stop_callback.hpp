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

#ifndef AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP
#define AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/stop_callback_lifetime.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver, class StopFunction>
class ReceiverAndStopCallback : private StopCallbackLifetime<exec::stop_token_type_t<Receiver&>, StopFunction>
{
  private:
    using StopToken = exec::stop_token_type_t<Receiver&>;
    using Base = StopCallbackLifetime<StopToken, StopFunction>;

  public:
    using Base::IS_STOPPABLE;

    template <class R>
    explicit ReceiverAndStopCallback(R&& receiver) : receiver_(static_cast<R&&>(receiver))
    {
    }

    Receiver& receiver() noexcept { return receiver_; }

    void reset_stop_callback() noexcept { this->reset(); }

    template <class Initiation, class Implementation>
    void emplace_stop_callback(StopToken&& stop_token, const Initiation& initiation,
                               Implementation& implementation) noexcept
    {
        if constexpr (Base::IS_STOPPABLE)
        {
            this->emplace(static_cast<StopToken&&>(stop_token), stop_function_arg(initiation, implementation));
        }
    }

  private:
    template <class Initiation, class Implementation>
    auto stop_function_arg(const Initiation& initiation, Implementation& implementation)
        -> decltype(initiation.stop_function_arg(implementation))
    {
        return initiation.stop_function_arg(implementation);
    }

    template <class Initiation, class Implementation>
    auto stop_function_arg(const Initiation& initiation, const Implementation&)
        -> decltype(initiation.stop_function_arg())
    {
        return initiation.stop_function_arg();
    }

    Receiver receiver_;
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RECEIVER_AND_STOP_CALLBACK_HPP
