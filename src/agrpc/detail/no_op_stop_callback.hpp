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

#ifndef AGRPC_DETAIL_NO_OP_STOP_CALLBACK_HPP
#define AGRPC_DETAIL_NO_OP_STOP_CALLBACK_HPP

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class NoOpStopCallback
{
  public:
    template <class StopToken>
    static constexpr void emplace(StopToken&&) noexcept
    {
    }

    [[nodiscard]] static constexpr bool is_stopped() noexcept { return false; }

    static constexpr void reset() noexcept {}
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NO_OP_STOP_CALLBACK_HPP
