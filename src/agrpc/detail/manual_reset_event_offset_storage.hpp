// Copyright 2024 Dennis Hezel
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

#ifndef AGRPC_DETAIL_MANUAL_RESET_EVENT_OFFSET_STORAGE_HPP
#define AGRPC_DETAIL_MANUAL_RESET_EVENT_OFFSET_STORAGE_HPP

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/tuple.hpp>

#include <cstddef>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <std::ptrdiff_t Offset, class T>
class ManualResetEventOffsetStorage
{
  public:
    static constexpr auto OFFSET = Offset;

    void set_value(T&& arg) { value() = static_cast<T&&>(arg); }

    auto get_value() && noexcept { return detail::Tuple<T>{static_cast<T&&>(value())}; }

  private:
    T& value() noexcept { return *reinterpret_cast<T*>(reinterpret_cast<std::byte*>(this) + Offset); }
};

template <std::ptrdiff_t Offset>
struct ManualResetEventOffsetStorageTemplate
{
    template <class T>
    using Type = ManualResetEventOffsetStorage<Offset, T>;
};

template <class Signature, std::ptrdiff_t Offset>
using OffsetManualResetEvent =
    detail::BasicManualResetEvent<Signature, ManualResetEventOffsetStorageTemplate<Offset>::template Type>;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_MANUAL_RESET_EVENT_OFFSET_STORAGE_HPP
