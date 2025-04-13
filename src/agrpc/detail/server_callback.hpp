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

#ifndef AGRPC_DETAIL_SERVER_CALLBACK_HPP
#define AGRPC_DETAIL_SERVER_CALLBACK_HPP

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/manual_reset_event_offset_storage.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#define AGRPC_STORAGE_HAS_CORRECT_OFFSET(D, E, S) \
    static_assert(decltype(std::declval<D>().E)::Storage::OFFSET == offsetof(D, S) - offsetof(D, E))

inline constexpr auto OFFSET_MANUAL_RESET_EVENT_SIZE = sizeof(detail::OffsetManualResetEvent<void(bool), 0>);

struct ServerUnaryReactorData
{
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> finish_{};
    bool ok_initial_metadata_{};
    bool ok_finish_{};
    bool is_finished_{};
};

static_assert(std::is_standard_layout_v<ServerUnaryReactorData>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerUnaryReactorData, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerUnaryReactorData, finish_, ok_finish_);

#undef AGRPC_STORAGE_HAS_CORRECT_OFFSET
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_CALLBACK_HPP
