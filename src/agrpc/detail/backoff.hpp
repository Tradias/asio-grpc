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

#ifndef AGRPC_DETAIL_BACKOFF_HPP
#define AGRPC_DETAIL_BACKOFF_HPP

#include "agrpc/detail/config.hpp"

#include <chrono>
#include <cstdint>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class T>
constexpr auto maximum(T a, T b) noexcept
{
    return (a < b) ? b : a;
}

using BackoffDelay = std::chrono::nanoseconds;

template <detail::BackoffDelay::rep MaxDelay>
class Backoff
{
  private:
    using Delay = detail::BackoffDelay;
    using Iteration = std::int_fast8_t;

  public:
    static constexpr Delay MAX_DELAY{MaxDelay};

    constexpr Delay next() noexcept
    {
        ++iterations;
        if (Iteration{5} == iterations)
        {
            iterations = Iteration{};
            increase_delay();
        }
        return delay;
    }

    constexpr void reset() noexcept { delay = Delay::zero(); }

  private:
    constexpr void increase_delay() noexcept
    {
        static constexpr Delay INCREMENT{detail::maximum(Delay::rep{1}, MaxDelay / 5)};
        if (MAX_DELAY > delay)
        {
            delay += INCREMENT;
        }
    }

    Iteration iterations{};
    Delay delay{Delay::zero()};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_BACKOFF_HPP
