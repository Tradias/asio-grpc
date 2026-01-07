// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_DETAIL_CLIENT_CALLBACK_HPP
#define AGRPC_DETAIL_CLIENT_CALLBACK_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <agrpc/detail/offset_manual_reset_event.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/status.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class StubAsync, class Request, class Response>
using AsyncUnaryFn = void (StubAsync::*)(grpc::ClientContext*, const Request*, Response*,
                                         std::function<void(::grpc::Status)>);

template <class StubAsync, class Request, class Response>
using AsyncUnaryReactorFn = void (StubAsync::*)(grpc::ClientContext*, const Request*, Response*,
                                                grpc::ClientUnaryReactor*);

template <class StubAsync, class Request, class Response>
using AsyncClientStreamingReactorFn = void (StubAsync::*)(grpc::ClientContext*, Response*,
                                                          grpc::ClientWriteReactor<Request>*);

template <class StubAsync, class Request, class Response>
using AsyncServerStreamingReactorFn = void (StubAsync::*)(grpc::ClientContext*, const Request*,
                                                          grpc::ClientReadReactor<Response>*);

template <class StubAsync, class Request, class Response>
using AsyncBidiStreamingReactorFn = void (StubAsync::*)(grpc::ClientContext*,
                                                        grpc::ClientBidiReactor<Request, Response>*);

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
template <class CompletionHandler>
class UnaryRequestCallback
{
  private:
    using WorkTracker = detail::WorkTracker<assoc::associated_executor_t<CompletionHandler>>;

    struct State : WorkTracker
    {
        explicit State(CompletionHandler&& handler)
            : WorkTracker(assoc::get_associated_executor(handler)), handler_(static_cast<CompletionHandler&&>(handler))
        {
        }

        CompletionHandler handler_;
    };

    struct PtrLikeState : State
    {
        using State::State;

        State& operator*() { return *this; }

        static void reset() {}
    };

    static auto create(CompletionHandler&& handler)
    {
        if constexpr (std::is_copy_constructible_v<State>)
        {
            return PtrLikeState{static_cast<CompletionHandler&&>(handler)};
        }
        else
        {
            auto allocator = assoc::get_associated_allocator(handler);
            return std::allocate_shared<State>(allocator, static_cast<CompletionHandler&&>(handler));
        }
    }

    using Storage = decltype(create(std::declval<CompletionHandler&&>()));

  public:
    explicit UnaryRequestCallback(CompletionHandler&& handler)
        : storage_(create(static_cast<CompletionHandler&&>(handler)))
    {
    }

    void operator()(grpc::Status status)
    {
        struct DispatchCallback
        {
            using allocator_type = asio::associated_allocator_t<CompletionHandler>;

            void operator()() { std::move(handler_)(std::move(status_)); }

            allocator_type get_allocator() const noexcept { return assoc::get_associated_allocator(handler_); }

            CompletionHandler handler_;
            grpc::Status status_;
        };
        auto state = std::move(*storage_);
        storage_.reset();
        auto executor = assoc::get_associated_executor(state.handler_);
        asio::dispatch(std::move(executor), DispatchCallback{std::move(state.handler_), std::move(status)});
    }

  private:
    Storage storage_;
};
#endif

#define AGRPC_STORAGE_HAS_CORRECT_OFFSET(D, E, S) \
    static_assert(decltype(std::declval<D>().E)::Storage::OFFSET == offsetof(D, S) - offsetof(D, E))

struct ClientUnaryReactorData
{
    detail::ManualResetEvent<void(bool)> initial_metadata_{};
    detail::ManualResetEvent<void(grpc::Status)> finish_{};
};

struct ClientWriteReactorDataBase
{
    detail::OffsetManualResetEvent<void(bool), 3 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> write_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + 2 * sizeof(bool)> writes_done_{};
    bool ok_initial_metadata_{};
    bool ok_write_{};
    bool ok_writes_done_{};
    std::atomic_bool is_hold_removed_{};
};

static_assert(std::is_standard_layout_v<ClientWriteReactorDataBase>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientWriteReactorDataBase, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientWriteReactorDataBase, write_, ok_write_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientWriteReactorDataBase, writes_done_, ok_writes_done_);

struct ClientWriteReactorData : ClientWriteReactorDataBase
{
    detail::ManualResetEvent<void(grpc::Status)> finish_{};
};

struct ClientReadReactorDataBase
{
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> read_{};
    bool ok_initial_metadata_{};
    bool ok_read_{};
    std::atomic_bool is_hold_removed_{};
};

static_assert(std::is_standard_layout_v<ClientReadReactorDataBase>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientReadReactorDataBase, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientReadReactorDataBase, read_, ok_read_);

struct ClientReadReactorData : ClientReadReactorDataBase
{
    detail::ManualResetEvent<void(grpc::Status)> finish_{};
};

struct ClientBidiReactorDataBase
{
    detail::OffsetManualResetEvent<void(bool), 4 * OFFSET_MANUAL_RESET_EVENT_SIZE> initial_metadata_{};
    detail::OffsetManualResetEvent<void(bool), 3 * OFFSET_MANUAL_RESET_EVENT_SIZE + sizeof(bool)> read_{};
    detail::OffsetManualResetEvent<void(bool), 2 * OFFSET_MANUAL_RESET_EVENT_SIZE + 2 * sizeof(bool)> write_{};
    detail::OffsetManualResetEvent<void(bool), OFFSET_MANUAL_RESET_EVENT_SIZE + 3 * sizeof(bool)> writes_done_{};
    bool ok_initial_metadata_{};
    bool ok_read_{};
    bool ok_write_{};
    bool ok_writes_done_{};
    std::atomic_bool is_hold_removed_{};
};

static_assert(std::is_standard_layout_v<ClientBidiReactorDataBase>);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientBidiReactorDataBase, initial_metadata_, ok_initial_metadata_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientBidiReactorDataBase, read_, ok_read_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientBidiReactorDataBase, write_, ok_write_);
AGRPC_STORAGE_HAS_CORRECT_OFFSET(ClientBidiReactorDataBase, writes_done_, ok_writes_done_);

struct ClientBidiReactorData : ClientBidiReactorDataBase
{
    detail::ManualResetEvent<void(grpc::Status)> finish_{};
};

#undef AGRPC_STORAGE_HAS_CORRECT_OFFSET
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_CLIENT_CALLBACK_HPP
