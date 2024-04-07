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

#ifndef AGRPC_DETAIL_BACKOFF_HPP
#define AGRPC_DETAIL_BACKOFF_HPP

#include <agrpc/detail/math.hpp>

#include <chrono>
#include <cstdint>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
using BackoffDelay = std::chrono::nanoseconds;

template <detail::BackoffDelay::rep MaxDelay>
class Backoff
{
  private:
    using Delay = detail::BackoffDelay;
    using Iteration = std::ptrdiff_t;

  public:
    static constexpr Delay MAX_DELAY{MaxDelay};
    static constexpr Iteration ITERATIONS_PER_DELAY{5};

    Delay next() noexcept
    {
        ++iterations_;
        if (ITERATIONS_PER_DELAY == iterations_)
        {
            iterations_ = Iteration{};
            increase_delay();
        }
        return delay_;
    }

    auto reset() noexcept
    {
        iterations_ = Iteration{};
        delay_ = Delay::zero();
        return delay_;
    }

  private:
    static constexpr Delay INCREMENT{detail::maximum(Delay::rep{1}, MaxDelay)};

    void increase_delay() noexcept
    {
        if (MAX_DELAY > delay_)
        {
            delay_ += INCREMENT;
        }
    }

    Delay delay_{Delay::zero()};
    Iteration iterations_{};
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_BACKOFF_HPP
