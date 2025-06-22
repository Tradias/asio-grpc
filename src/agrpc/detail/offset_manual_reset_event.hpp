// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_OFFSET_MANUAL_RESET_EVENT_HPP
#define AGRPC_DETAIL_OFFSET_MANUAL_RESET_EVENT_HPP

#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/manual_reset_event_offset_storage.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <std::ptrdiff_t Offset>
struct ManualResetEventOffsetStorageTemplate
{
    template <class T>
    using Type = ManualResetEventOffsetStorage<Offset, T>;
};

template <class Signature, std::ptrdiff_t Offset>
using OffsetManualResetEvent =
    detail::BasicManualResetEvent<Signature, ManualResetEventOffsetStorageTemplate<Offset>::template Type>;

inline constexpr auto OFFSET_MANUAL_RESET_EVENT_SIZE = sizeof(detail::OffsetManualResetEvent<void(bool), 0>);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_OFFSET_MANUAL_RESET_EVENT_HPP
