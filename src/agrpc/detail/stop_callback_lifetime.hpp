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

#ifndef AGRPC_DETAIL_STOP_CALLBACK_LIFETIME_HPP
#define AGRPC_DETAIL_STOP_CALLBACK_LIFETIME_HPP

#include <agrpc/detail/asio_association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>

#include <optional>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class StopToken, class StopFunction>
inline constexpr bool NEEDS_STOP_CALLBACK = detail::IS_STOP_EVER_POSSIBLE_V<StopToken>;

template <class StopToken>
inline constexpr bool NEEDS_STOP_CALLBACK<StopToken, detail::Empty> = false;

template <class StopToken, class StopFunction, bool = detail::NEEDS_STOP_CALLBACK<StopToken, StopFunction>>
class StopCallbackLifetime
{
  private:
    using StopCallback = std::optional<typename StopToken::template callback_type<StopFunction>>;

  public:
    static constexpr bool IS_STOPPABLE = true;

    void reset() noexcept { stop_callback_.reset(); }

    template <class... Args>
    void emplace(StopToken&& stop_token, Args&&... args) noexcept
    {
        if (stop_token.stop_possible())
        {
            stop_callback_.emplace(static_cast<StopToken&&>(stop_token), static_cast<Args&&>(args)...);
        }
    }

  private:
    StopCallback stop_callback_;
};

template <class StopToken, class StopFunction>
class StopCallbackLifetime<StopToken, StopFunction, false>
{
  public:
    static constexpr bool IS_STOPPABLE = false;

    static constexpr void reset() noexcept {}

    template <class... Args>
    static constexpr void emplace(const StopToken&, Args&&...) noexcept
    {
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_STOP_CALLBACK_LIFETIME_HPP
