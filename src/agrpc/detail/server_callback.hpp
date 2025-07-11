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

#ifndef AGRPC_DETAIL_SERVER_CALLBACK_HPP
#define AGRPC_DETAIL_SERVER_CALLBACK_HPP

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/offset_manual_reset_event.hpp>

#include <cstdint>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
class ReactorRPCState
{
  private:
    static constexpr std::uint8_t FINISH_CALLED_BIT = 1u << 0u;
    static constexpr std::uint8_t CANCELLED_BIT = 1u << 1u;

  public:
    [[nodiscard]] bool is_finish_called() const noexcept { return (state_ & FINISH_CALLED_BIT) != 0u; }

    void set_finish_called() noexcept { state_ |= FINISH_CALLED_BIT; }

    [[nodiscard]] bool is_cancelled() const noexcept { return (state_ & CANCELLED_BIT) != 0u; }

    void set_cancelled() noexcept { state_ |= CANCELLED_BIT; }

  private:
    std::atomic<std::uint8_t> state_{};
};

#define AGRPC_STORAGE_HAS_CORRECT_OFFSET(D, E, S) \
    static_assert(decltype(std::declval<D>().E)::Storage::OFFSET == offsetof(D, S) - offsetof(D, E))

struct ServerUnaryReactorData
{
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> finish_{};
    bool ok_initial_metadata_{};
    bool ok_finish_{};
    ReactorRPCState state_{};
};

static_assert(std::is_standard_layout_v<ServerUnaryReactorData>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerUnaryReactorData, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerUnaryReactorData, finish_, ok_finish_);

struct ServerReadReactorData
{
    detail::OffsetManualResetEvent<void(bool), 3 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> read_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + 2 * sizeof(bool)> finish_{};
    bool ok_initial_metadata_{};
    bool ok_read_{};
    bool ok_finish_{};
    ReactorRPCState state_{};
};

static_assert(std::is_standard_layout_v<ServerReadReactorData>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerReadReactorData, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerReadReactorData, read_, ok_read_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerReadReactorData, finish_, ok_finish_);

struct ServerWriteReactorData
{
    detail::OffsetManualResetEvent<void(bool), 3 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> write_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + 2 * sizeof(bool)> finish_{};
    bool ok_initial_metadata_{};
    bool ok_write_{};
    bool ok_finish_{};
    ReactorRPCState state_{};
};

static_assert(std::is_standard_layout_v<ServerWriteReactorData>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerWriteReactorData, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerWriteReactorData, write_, ok_write_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerWriteReactorData, finish_, ok_finish_);

struct ServerBidiReactorData
{
    detail::OffsetManualResetEvent<void(bool), 4 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), 3 * OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> read_{};
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE + 2 * sizeof(bool)> write_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + 3 * sizeof(bool)> finish_{};
    bool ok_initial_metadata_{};
    bool ok_read_{};
    bool ok_write_{};
    bool ok_finish_{};
    ReactorRPCState state_{};
};

static_assert(std::is_standard_layout_v<ServerBidiReactorData>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerBidiReactorData, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerBidiReactorData, write_, ok_write_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ServerBidiReactorData, finish_, ok_finish_);

#undef AGRPC_STORAGE_HAS_CORRECT_OFFSET
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_CALLBACK_HPP
