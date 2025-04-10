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

#ifndef AGRPC_AGRPC_SERVER_CALLBACK_HPP
#define AGRPC_AGRPC_SERVER_CALLBACK_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/manual_reset_event.hpp>
#include <grpcpp/support/server_callback.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

template <class Executor>
class BasicServerUnaryReactor : private grpc::ServerUnaryReactor
{
  public:
    using executor_type = Executor;

    [[nodiscard]] grpc::ServerUnaryReactor* get() noexcept { return this; }

    [[nodiscard]] const Executor& get_executor() const noexcept { return executor_; }

    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return initial_metadata_.wait(static_cast<CompletionToken&&>(token), executor_);
    }

    void initiate_finish(grpc::Status status)
    {
        is_finished_ = true;
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return finish_.wait(static_cast<CompletionToken&&>(token), executor_);
    }

  private:
    template <class>
    friend class detail::RefCountedReactor;

    explicit BasicServerUnaryReactor(Executor executor) : executor_(static_cast<Executor&&>(executor)) {}

    [[nodiscard]] bool is_finished() const noexcept { return is_finished_; }

    void OnSendInitialMetadataDone(bool ok) final { initial_metadata_.set(static_cast<bool&&>(ok)); }

    void on_done()
    {
        is_finished_ = true;
        finish_.set(true);
    }

    void OnCancel() final { finish_.set(false); }

    detail::ManualResetEvent<void(bool)> initial_metadata_{};
    detail::ManualResetEvent<void(bool)> finish_{};
    Executor executor_;
    bool is_finished_{};
};

using ServerUnaryReactor = BasicServerUnaryReactor<asio::any_io_executor>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_CALLBACK_HPP
