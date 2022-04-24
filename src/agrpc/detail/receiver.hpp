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

#ifndef AGRPC_DETAIL_RECEIVER_HPP
#define AGRPC_DETAIL_RECEIVER_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>

#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Receiver, class... Args>
void satisfy_receiver(Receiver&& receiver, Args&&... args) noexcept
{
    if constexpr (noexcept(detail::exec::set_value(std::forward<Receiver>(receiver), std::forward<Args>(args)...)))
    {
        detail::exec::set_value(std::forward<Receiver>(receiver), std::forward<Args>(args)...);
    }
    else
    {
        AGRPC_TRY { detail::exec::set_value(std::forward<Receiver>(receiver), std::forward<Args>(args)...); }
        AGRPC_CATCH(...) { detail::exec::set_error(std::forward<Receiver>(receiver), std::current_exception()); }
    }
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_RECEIVER_HPP
