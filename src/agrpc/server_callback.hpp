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
#include <agrpc/detail/reactor_executor_base.hpp>
#include <agrpc/detail/server_callback.hpp>
#include <grpcpp/support/server_callback.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

// Unary
template <class Executor>
class BasicServerUnaryReactor : private grpc::ServerUnaryReactor, public detail::ReactorExecutorBase<Executor>
{
  public:
    [[nodiscard]] grpc::ServerUnaryReactor* get() noexcept { return this; }

    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    void initiate_finish(grpc::Status status)
    {
        data_.is_finished_ = true;
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    template <class>
    friend class detail::RefCountedReactor;

    using BasicServerUnaryReactor::ReactorExecutorBase::ReactorExecutorBase;

    [[nodiscard]] bool is_finished() const noexcept { return data_.is_finished_; }

    void OnSendInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void on_done()
    {
        data_.is_finished_ = true;
        data_.finish_.set(true);
    }

    void OnCancel() final { data_.finish_.set(false); }

    detail::ServerUnaryReactorData data_;
};

using ServerUnaryReactor = BasicServerUnaryReactor<asio::any_io_executor>;

// Client-streaming
template <class Request, class Executor>
class BasicServerReadReactor : private grpc::ServerReadReactor<Request>, public detail::ReactorExecutorBase<Executor>
{
  public:
    [[nodiscard]] grpc::ServerUnaryReactor* get() noexcept { return this; }

    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    void initiate_read(Request& request)
    {
        data_.read_.reset();
        this->StartRead(&request);
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_read(CompletionToken&& token = CompletionToken{})
    {
        return data_.read_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    void initiate_finish(grpc::Status status)
    {
        data_.is_finished_ = true;
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    template <class>
    friend class detail::RefCountedReactor;

    using BasicServerReadReactor::ReactorExecutorBase::ReactorExecutorBase;

    [[nodiscard]] bool is_finished() const noexcept { return data_.is_finished_; }

    void OnSendInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnReadDone(bool ok) final { data_.read_.set(static_cast<bool&&>(ok)); }

    void on_done()
    {
        data_.is_finished_ = true;
        data_.finish_.set(true);
    }

    void OnCancel() final { data_.finish_.set(false); }

    detail::ServerReadReactorData data_;
};

template <class Request>
using ServerReadReactor = BasicServerReadReactor<Request, asio::any_io_executor>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_CALLBACK_HPP
