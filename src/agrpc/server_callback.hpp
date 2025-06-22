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

#ifndef AGRPC_AGRPC_SERVER_CALLBACK_HPP
#define AGRPC_AGRPC_SERVER_CALLBACK_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/reactor_executor_base.hpp>
#include <agrpc/detail/ref_counted_reactor.hpp>
#include <agrpc/detail/server_callback.hpp>
#include <grpcpp/support/server_callback.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief I/O object for server-side, unary rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor` or
 * `server_callback_coroutine.hpp`. Note that `grpc::CallbackServerContext::DefaultReactor()` should be use instead
 * of this class whenever possible.
 *
 * Example:
 *
 * @snippet server_callback.cpp server-rpc-unary-callback
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam Executor The executor type.
 *
 * **Per-Operation Cancellation**
 *
 * All. Cancellation will merely interrupt the act of waiting and does not cancel the underlying rpc.
 *
 * @since 3.5.0
 */
template <class Executor>
class BasicServerUnaryReactor : private grpc::ServerUnaryReactor, public detail::ReactorExecutorBase<Executor>
{
  public:
    /**
     * @brief Rebind the BasicServerUnaryReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicServerUnaryReactor type when rebound to the specified executor
         */
        using other = BasicServerUnaryReactor<OtherExecutor>;
    };

    /**
     * @brief Get underlying gRPC reactor
     *
     * The returned object should be passed to the gRPC library. Invoking any of its functions may result in undefined
     * behavior.
     */
    [[nodiscard]] grpc::ServerUnaryReactor* get() noexcept { return this; }

    /**
     * @brief Send initial metadata
     *
     * Send any initial metadata stored in the CallbackServerContext. If not invoked, any initial metadata will be
     * passed along with `initiate_finish`.
     */
    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    /**
     * @brief Wait for send initial metadata
     *
     * Waits for the completion of `initiate_send_initial_metadata`. Only one wait for send initial metadata may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Finish rpc
     *
     * Indicate that the stream is to be finished and the trailing metadata and rpc status are to be sent. May only be
     * called once. If the status is non-OK, any message will not be sent. Instead, the client will only receive the
     * status and any trailing metadata.
     */
    void initiate_finish(grpc::Status status)
    {
        data_.state_.set_finish_called();
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. Only one wait for finish may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done()
    {
        if (!data_.state_.is_finish_called())
        {
            initiate_finish({grpc::StatusCode::CANCELLED, {}});
        }
    }

    void OnSendInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void on_done() { data_.finish_.set(!data_.state_.is_cancelled()); }

    void OnCancel() final
    {
        data_.state_.set_cancelled();
        data_.finish_.set(false);
    }

    detail::ServerUnaryReactorData data_;
};

/**
 * @brief I/O object for server-side, unary rpcs (specialized on `asio::any_io_executor`)
 */
using ServerUnaryReactor = BasicServerUnaryReactor<asio::any_io_executor>;

template <class Executor>
using BasicServerUnaryReactorBase = detail::RefCountedServerReactor<agrpc::BasicServerUnaryReactor<Executor>>;

using ServerUnaryReactorBase = BasicServerUnaryReactorBase<asio::any_io_executor>;

/**
 * @brief I/O object for server-side, client-streaming rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor` or
 * `server_callback_coroutine.hpp`.
 *
 * Example:
 *
 * @snippet server_callback.cpp server-rpc-client-streaming-callback
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam Executor The executor type.
 *
 * **Per-Operation Cancellation**
 *
 * All. Cancellation will merely interrupt the act of waiting and does not cancel the underlying rpc.
 *
 * @since 3.5.0
 */
template <class Request, class Executor>
class BasicServerReadReactor : private grpc::ServerReadReactor<Request>, public detail::ReactorExecutorBase<Executor>
{
  public:
    /**
     * @brief Rebind the BasicServerReadReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicServerReadReactor type when rebound to the specified executor
         */
        using other = BasicServerReadReactor<Request, OtherExecutor>;
    };

    /**
     * @brief Get underlying gRPC reactor
     *
     * The returned object should be passed to the gRPC library. Invoking any of its functions may result in undefined
     * behavior.
     */
    [[nodiscard]] grpc::ServerReadReactor<Request>* get() noexcept { return this; }

    /**
     * @brief Send initial metadata
     *
     * Send any initial metadata stored in the CallbackServerContext. If not invoked, any initial metadata will be
     * passed along with `initiate_finish`.
     */
    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    /**
     * @brief Wait for send initial metadata
     *
     * Waits for the completion of `initiate_send_initial_metadata`. Only one wait for send initial metadata may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Read message
     *
     * Initiate the read of a message from the client. The argument must remain valid until the read completes
     * (`wait_for_read()`).
     */
    void initiate_read(Request& request)
    {
        data_.read_.reset();
        this->StartRead(&request);
    }

    /**
     * @brief Wait for read
     *
     * Waits for the completion of a read. Only one wait for read may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_read(CompletionToken&& token = CompletionToken{})
    {
        return data_.read_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Finish rpc
     *
     * Indicate that the stream is to be finished and the trailing metadata and rpc status are to be sent. May only be
     * called once. If the status is non-OK, any message will not be sent. Instead, the client will only receive the
     * status and any trailing metadata.
     */
    void initiate_finish(grpc::Status status)
    {
        data_.state_.set_finish_called();
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. Only one wait for finish may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done()
    {
        if (!data_.state_.is_finish_called())
        {
            initiate_finish({grpc::StatusCode::CANCELLED, {}});
        }
    }

    void OnSendInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnReadDone(bool ok) final { data_.read_.set(static_cast<bool&&>(ok)); }

    void on_done() { data_.finish_.set(!data_.state_.is_cancelled()); }

    void OnCancel() final
    {
        data_.state_.set_cancelled();
        data_.finish_.set(false);
    }

    detail::ServerReadReactorData data_;
};

/**
 * @brief I/O object for server-side, client-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Request>
using ServerReadReactor = BasicServerReadReactor<Request, asio::any_io_executor>;

template <class Request, class Executor>
using BasicServerReadReactorBase = detail::RefCountedServerReactor<agrpc::BasicServerReadReactor<Request, Executor>>;

template <class Request>
using ServerReadReactorBase = BasicServerReadReactorBase<Request, asio::any_io_executor>;

/**
 * @brief I/O object for server-side, server-streaming rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor` or
 * `server_callback_coroutine.hpp`.
 *
 * Example:
 *
 * @snippet server_callback.cpp server-rpc-server-streaming-callback
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam Executor The executor type.
 *
 * **Per-Operation Cancellation**
 *
 * All. Cancellation will merely interrupt the act of waiting and does not cancel the underlying rpc.
 *
 * @since 3.5.0
 */
template <class Response, class Executor>
class BasicServerWriteReactor : private grpc::ServerWriteReactor<Response>, public detail::ReactorExecutorBase<Executor>
{
  public:
    /**
     * @brief Rebind the BasicServerWriteReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicServerWriteReactor type when rebound to the specified executor
         */
        using other = BasicServerWriteReactor<Response, OtherExecutor>;
    };

    /**
     * @brief Get underlying gRPC reactor
     *
     * The returned object should be passed to the gRPC library. Invoking any of its functions may result in undefined
     * behavior.
     */
    [[nodiscard]] grpc::ServerWriteReactor<Response>* get() noexcept { return this; }

    /**
     * @brief Send initial metadata
     *
     * Send any initial metadata stored in the CallbackServerContext. If not invoked, any initial metadata will be
     * passed along with `initiate_finish`.
     */
    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    /**
     * @brief Wait for send initial metadata
     *
     * Waits for the completion of `initiate_send_initial_metadata`. Only one wait for send initial metadata may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Write message
     *
     * Initiate the read of a message from the client. The argument must remain valid until the read completes
     * (`wait_for_write()`).
     */
    void initiate_write(const Response& response, grpc::WriteOptions options = {})
    {
        data_.write_.reset();
        this->StartWrite(&response, options);
    }

    /**
     * @brief Wait for read
     *
     * Waits for the completion of a write. Only one wait for read may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_write(CompletionToken&& token = CompletionToken{})
    {
        return data_.write_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Write message and finish rpc
     *
     * Initiate a write operation with specified options and final rpc status, which also causes any trailing metadata
     * for this rpc to be sent out. The argument must remain valid until the rpc completes (`wait_for_finish()`). Either
     * `initiate_write_and_finish()` or `initiate_finish()` may be called but not both.
     */
    void initiate_write_and_finish(const Response& response, grpc::Status status, grpc::WriteOptions options = {})
    {
        data_.state_.set_finish_called();
        this->StartWriteAndFinish(&response, options, static_cast<grpc::Status&&>(status));
    }

    /**
     * @brief Finish rpc
     *
     * Indicate that the stream is to be finished and the trailing metadata and rpc status are to be sent. May only be
     * called once. If the status is non-OK, any message will not be sent. Instead, the client will only receive the
     * status and any trailing metadata. Either `initiate_write_and_finish()` or `initiate_finish()` may be called but
     * not both.
     */
    void initiate_finish(grpc::Status status)
    {
        data_.state_.set_finish_called();
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. Only one wait for finish may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done()
    {
        if (!data_.state_.is_finish_called())
        {
            initiate_finish({grpc::StatusCode::CANCELLED, {}});
        }
    }

    void OnSendInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnWriteDone(bool ok) final { data_.write_.set(static_cast<bool&&>(ok)); }

    void on_done() { data_.finish_.set(!data_.state_.is_cancelled()); }

    void OnCancel() final
    {
        data_.state_.set_cancelled();
        data_.finish_.set(false);
    }

    detail::ServerWriteReactorData data_;
};

/**
 * @brief I/O object for server-side, server-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Response>
using ServerWriteReactor = BasicServerWriteReactor<Response, asio::any_io_executor>;

template <class Response, class Executor>
using BasicServerWriteReactorBase = detail::RefCountedServerReactor<agrpc::BasicServerWriteReactor<Response, Executor>>;

template <class Response>
using ServerWriteReactorBase = BasicServerWriteReactorBase<Response, asio::any_io_executor>;

/**
 * @brief I/O object for server-side, bidi-streaming rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor` or
 * `server_callback_coroutine.hpp`.
 *
 * Example:
 *
 * @snippet server_callback.cpp server-rpc-bidi-streaming-callback
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * @tparam Executor The executor type.
 *
 * **Per-Operation Cancellation**
 *
 * All. Cancellation will merely interrupt the act of waiting and does not cancel the underlying rpc.
 *
 * @since 3.5.0
 */
template <class Request, class Response, class Executor>
class BasicServerBidiReactor : private grpc::ServerBidiReactor<Request, Response>,
                               public detail::ReactorExecutorBase<Executor>
{
  public:
    /**
     * @brief Rebind the BasicServerBidiReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicServerBidiReactor type when rebound to the specified executor
         */
        using other = BasicServerBidiReactor<Request, Response, OtherExecutor>;
    };

    /**
     * @brief Get underlying gRPC reactor
     *
     * The returned object should be passed to the gRPC library. Invoking any of its functions may result in undefined
     * behavior.
     */
    [[nodiscard]] grpc::ServerBidiReactor<Request, Response>* get() noexcept { return this; }

    /**
     * @brief Send initial metadata
     *
     * Send any initial metadata stored in the CallbackServerContext. If not invoked, any initial metadata will be
     * passed along with `initiate_finish`.
     */
    void initiate_send_initial_metadata() { this->StartSendInitialMetadata(); }

    /**
     * @brief Wait for send initial metadata
     *
     * Waits for the completion of `initiate_send_initial_metadata`. Only one wait for send initial metadata may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_send_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Read message
     *
     * Initiate the read of a message from the client. The argument must remain valid until the read completes
     * (`wait_for_read()`).
     */
    void initiate_read(Request& request)
    {
        data_.read_.reset();
        this->StartRead(&request);
    }

    /**
     * @brief Wait for read
     *
     * Waits for the completion of a read. Only one wait for read may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_read(CompletionToken&& token = CompletionToken{})
    {
        return data_.read_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Write message
     *
     * Initiate the read of a message from the client. The argument must remain valid until the read completes
     * (`wait_for_write()`).
     */
    void initiate_write(const Response& response, grpc::WriteOptions options = {})
    {
        data_.write_.reset();
        this->StartWrite(&response, options);
    }

    /**
     * @brief Wait for read
     *
     * Waits for the completion of a write. Only one wait for read may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_write(CompletionToken&& token = CompletionToken{})
    {
        return data_.write_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Write message and finish rpc
     *
     * Initiate a write operation with specified options and final rpc status, which also causes any trailing metadata
     * for this rpc to be sent out. The argument must remain valid until the rpc completes (`wait_for_finish()`). Either
     * `initiate_write_and_finish()` or `initiate_finish()` may be called but not both.
     */
    void initiate_write_and_finish(const Response& response, grpc::Status status, grpc::WriteOptions options = {})
    {
        data_.state_.set_finish_called();
        this->StartWriteAndFinish(&response, options, static_cast<grpc::Status&&>(status));
    }

    /**
     * @brief Finish rpc
     *
     * Indicate that the stream is to be finished and the trailing metadata and rpc status are to be sent. May only be
     * called once. If the status is non-OK, any message will not be sent. Instead, the client will only receive the
     * status and any trailing metadata. Either `initiate_write_and_finish()` or `initiate_finish()` may be called but
     * not both.
     */
    void initiate_finish(grpc::Status status)
    {
        data_.state_.set_finish_called();
        this->Finish(static_cast<grpc::Status&&>(status));
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. Only one wait for finish may be
     * outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done()
    {
        if (!data_.state_.is_finish_called())
        {
            initiate_finish({grpc::StatusCode::CANCELLED, {}});
        }
    }

    void OnSendInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnReadDone(bool ok) final { data_.read_.set(static_cast<bool&&>(ok)); }

    void OnWriteDone(bool ok) final { data_.write_.set(static_cast<bool&&>(ok)); }

    void on_done() { data_.finish_.set(!data_.state_.is_cancelled()); }

    void OnCancel() final
    {
        data_.state_.set_cancelled();
        data_.finish_.set(false);
    }

    detail::ServerBidiReactorData data_;
};

/**
 * @brief I/O object for server-side, bidi-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Request, class Response>
using ServerBidiReactor = BasicServerBidiReactor<Request, Response, asio::any_io_executor>;

template <class Request, class Response, class Executor>
using BasicServerBidiReactorBase =
    detail::RefCountedServerReactor<agrpc::BasicServerBidiReactor<Request, Response, Executor>>;

template <class Request, class Response>
using ServerBidiReactorBase = BasicServerBidiReactorBase<Request, Response, asio::any_io_executor>;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_CALLBACK_HPP
