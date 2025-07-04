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

#ifndef AGRPC_AGRPC_CLIENT_CALLBACK_HPP
#define AGRPC_AGRPC_CLIENT_CALLBACK_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/bind_allocator.hpp>
#include <agrpc/detail/client_callback.hpp>
#include <agrpc/detail/default_completion_token.hpp>
#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/reactor_client_context_base.hpp>
#include <agrpc/detail/reactor_executor_base.hpp>
#include <grpcpp/generic/generic_stub_callback.h>
#include <grpcpp/support/client_callback.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief (experimental) I/O object for client-side, unary rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor`. This class should only be used
 * if the unary rpc wants to receive initial metadata without waiting for the server's response message.
 *
 * Example:
 *
 * @snippet client_callback.cpp client-rpc-unary-callback
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
class BasicClientUnaryReactor : private grpc::ClientUnaryReactor,
                                public detail::ReactorExecutorBase<Executor>,
                                public detail::ReactorClientContextBase
{
  public:
    /**
     * @brief Rebind the BasicClientUnaryReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicClientUnaryReactor type when rebound to the specified executor
         */
        using other = BasicClientUnaryReactor<OtherExecutor>;
    };

    /**
     * @brief Start a codegen-ed rpc
     *
     * The response object must remain valid until the rpc is finished. May only be called once.
     *
     * @arg fn Pointer to the protoc generated `Stub::async::Method`.
     */
    template <class StubAsync, class Request, class Response>
    void start(detail::AsyncUnaryReactorFn<StubAsync, Request, Response> fn, StubAsync* stub, const Request& request,
               Response& response)
    {
        (*stub.*fn)(&this->context(), &request, &response, static_cast<grpc::ClientUnaryReactor*>(this));
        this->StartCall();
    }

    template <class Request, class Response>
    void start(grpc::TemplatedGenericStubCallback<Request, Response>& stub, const std::string& method,
               const Request& request, Response& response, grpc::StubOptions options = {})
    {
        stub.PrepareUnaryCall(&this->context(), method, options, &request, &response,
                              static_cast<grpc::ClientUnaryReactor*>(this));
        this->StartCall();
    }

    /**
     * @brief Wait for initial metadata
     *
     * Only one wait for initial metadata may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. Only one wait for finish may be outstanding at
     * any time.
     *
     * Completion signature is `void(error_code, grpc::Status)`. Once this operation completes the response passed to
     * `start()` will have been be populated if `grpc::Status::ok()` is true.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  protected:
    BasicClientUnaryReactor() = default;

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    static void on_user_done() {}

    void OnReadInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void on_done(const grpc::Status& status) { data_.finish_.set(grpc::Status{status}); }

    detail::ClientUnaryReactorData data_;
};

/**
 * @brief (experimental) I/O object for client-side, unary rpcs (specialized on `asio::any_io_executor`)
 */
using ClientUnaryReactor = BasicClientUnaryReactor<asio::any_io_executor>;

template <class Executor>
using BasicClientUnaryReactorBase = detail::RefCountedClientReactor<agrpc::BasicClientUnaryReactor<Executor>>;

using ClientUnaryReactorBase = BasicClientUnaryReactorBase<asio::any_io_executor>;

/**
 * @brief (experimental) I/O object for client-side, client-streaming rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor`.
 *
 * Example:
 *
 * @snippet client_callback.cpp client-rpc-client-streaming-callback
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
class BasicClientWriteReactor : private grpc::ClientWriteReactor<Request>,
                                public detail::ReactorExecutorBase<Executor>,
                                public detail::ReactorClientContextBase
{
  public:
    /**
     * @brief Rebind the BasicClientWriteReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicClientWriteReactor type when rebound to the specified executor
         */
        using other = BasicClientWriteReactor<Request, OtherExecutor>;
    };

    /**
     * @brief Start a codegen-ed rpc
     *
     * The response object must remain valid until the rpc is finished. May only be called once.
     *
     * @arg fn Pointer to the protoc generated `Stub::async::Method`.
     */
    template <class StubAsync, class Response>
    void start(detail::AsyncClientStreamingReactorFn<StubAsync, Request, Response> fn, StubAsync* stub,
               Response& response)
    {
        (*stub.*fn)(&this->context(), &response, static_cast<grpc::ClientWriteReactor<Request>*>(this));
        this->AddHold();
        this->StartCall();
    }

    /**
     * @brief Wait for initial metadata
     *
     * Only one wait for initial metadata may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Write message
     *
     * Initiate the write of a message. The argument must remain valid until the write completes (`wait_for_write`).
     * If `WriteOptions::set_last_message()` is present then no more calls to `initiate_write` or `initiate_writes_done`
     * are allowed.
     */
    void initiate_write(const Request& request, grpc::WriteOptions options = {})
    {
        data_.write_.reset();
        this->StartWrite(&request, options);
    }

    /**
     * @brief Indicate that the rpc will have no more write operations
     *
     * This can only be issued once for a given rpc. This is not required or allowed if `initiate_write` with
     * `set_last_message()` is used since that already has the same implication. Note that calling this means that no
     * more calls to `initiate_write` or `initiate_writes_done` are allowed.
     */
    void initiate_writes_done()
    {
        remove_hold();
        this->StartWritesDone();
    }

    /**
     * @brief Wait for write
     *
     * Waits for the completion of a write. Only one wait for write may be outstanding at any time.
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
     * @brief Wait for writes done
     *
     * Waits for the completion of `writes_done`. Only one wait for write may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_writes_done(CompletionToken&& token = CompletionToken{})
    {
        return data_.writes_done_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. No more writes may be initiated on this rpc
     * after this function has been called. Only one wait for finish may be outstanding at any time.
     *
     * Completion signature is `void(error_code, grpc::Status)`. Once this operation completes the response passed to
     * `start()` will have been be populated if `grpc::Status::ok()` is true.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        remove_hold();
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  protected:
    BasicClientWriteReactor() = default;

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done() { remove_hold(); }

    void OnReadInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnWriteDone(bool ok) final { data_.write_.set(static_cast<bool&&>(ok)); }

    void OnWritesDoneDone(bool ok) final { data_.writes_done_.set(static_cast<bool&&>(ok)); }

    void on_done(const grpc::Status& status) { data_.finish_.set(grpc::Status{status}); }

    void remove_hold()
    {
        if (!data_.is_hold_removed_.exchange(true, std::memory_order_relaxed))
        {
            this->RemoveHold();
        }
    }

    detail::ClientWriteReactorData data_;
};

/**
 * @brief (experimental) I/O object for client-side, client-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Request>
using ClientWriteReactor = BasicClientWriteReactor<Request, asio::any_io_executor>;

template <class Request, class Executor>
using BasicClientWriteReactorBase = detail::RefCountedClientReactor<agrpc::BasicClientWriteReactor<Request, Executor>>;

template <class Request>
using ClientWriteReactorBase = BasicClientWriteReactorBase<Request, asio::any_io_executor>;

/**
 * @brief (experimental) I/O object for client-side, server-streaming rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor`.
 *
 * Example:
 *
 * @snippet client_callback.cpp client-rpc-server-streaming-callback
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
class BasicClientReadReactor : private grpc::ClientReadReactor<Response>,
                               public detail::ReactorExecutorBase<Executor>,
                               public detail::ReactorClientContextBase
{
  public:
    /**
     * @brief Rebind the BasicClientReadReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicClientReadReactor type when rebound to the specified executor
         */
        using other = BasicClientReadReactor<Response, OtherExecutor>;
    };

    /**
     * @brief Start a codegen-ed rpc
     *
     * The request object must remain valid until the rpc is finished. May only be called once.
     *
     * @arg fn Pointer to the protoc generated `Stub::async::Method`.
     */
    template <class StubAsync, class Request>
    void start(detail::AsyncServerStreamingReactorFn<StubAsync, Request, Response> fn, StubAsync* stub,
               const Request& request)
    {
        (*stub.*fn)(&this->context(), &request, static_cast<grpc::ClientReadReactor<Response>*>(this));
        this->AddHold();
        this->StartCall();
    }

    /**
     * @brief Wait for initial metadata
     *
     * Only one wait for initial metadata may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Read message
     *
     * Initiate the read of a message. The argument must remain valid until the write completes (`wait_for_read`).
     */
    void initiate_read(Response& response)
    {
        data_.read_.reset();
        this->StartRead(&response);
    }

    /**
     * @brief Wait for write
     *
     * Waits for the completion of a read. Only one wait for write may be outstanding at any time.
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
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. No more reads may be initiated on this rpc
     * after this function has been called. Only one wait for finish may be outstanding at any time.
     *
     * Completion signature is `void(error_code, grpc::Status)`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        remove_hold();
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  protected:
    BasicClientReadReactor() = default;

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done() { remove_hold(); }

    void OnReadInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnReadDone(bool ok) final { data_.read_.set(static_cast<bool&&>(ok)); }

    void on_done(const grpc::Status& status) { data_.finish_.set(grpc::Status{status}); }

    void remove_hold()
    {
        if (!data_.is_hold_removed_.exchange(true, std::memory_order_relaxed))
        {
            this->RemoveHold();
        }
    }

    detail::ClientReadReactorData data_;
};

/**
 * @brief (experimental) I/O object for client-side, server-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Response>
using ClientReadReactor = BasicClientReadReactor<Response, asio::any_io_executor>;

template <class Response, class Executor>
using BasicClientReadReactorBase = detail::RefCountedClientReactor<agrpc::BasicClientReadReactor<Response, Executor>>;

template <class Response>
using ClientReadReactorBase = BasicClientReadReactorBase<Response, asio::any_io_executor>;

/**
 * @brief (experimental) I/O object for client-side, bidi-streaming rpcs
 *
 * Create an object of this type using `agrpc::make_reactor`/`agrpc::allocate_reactor`.
 *
 * Example:
 *
 * @snippet client_callback.cpp client-rpc-bidi-streaming-callback
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
class BasicClientBidiReactor : private grpc::ClientBidiReactor<Request, Response>,
                               public detail::ReactorExecutorBase<Executor>,
                               public detail::ReactorClientContextBase
{
  public:
    /**
     * @brief Rebind the BasicClientBidiReactor to another executor
     */
    template <class OtherExecutor>
    struct rebind_executor
    {
        /**
         * @brief The BasicClientBidiReactor type when rebound to the specified executor
         */
        using other = BasicClientBidiReactor<Request, Response, OtherExecutor>;
    };

    /**
     * @brief Start a codegen-ed rpc
     *
     * The response object must remain valid until the rpc is finished. May only be called once.
     *
     * @arg fn Pointer to the protoc generated `Stub::async::Method`.
     */
    template <class StubAsync>
    void start(detail::AsyncBidiStreamingReactorFn<StubAsync, Request, Response> fn, StubAsync* stub)
    {
        (*stub.*fn)(&this->context(), static_cast<grpc::ClientBidiReactor<Request, Response>*>(this));
        this->AddHold();
        this->StartCall();
    }

    /**
     * @brief Wait for initial metadata
     *
     * Only one wait for initial metadata may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_initial_metadata(CompletionToken&& token = CompletionToken{})
    {
        return data_.initial_metadata_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Read message
     *
     * Initiate the read of a message. The argument must remain valid until the write completes (`wait_for_read`).
     */
    void initiate_read(Response& response)
    {
        data_.read_.reset();
        this->StartRead(&response);
    }

    /**
     * @brief Wait for write
     *
     * Waits for the completion of a read. Only one wait for write may be outstanding at any time.
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
     * Initiate the write of a message. The argument must remain valid until the write completes (`wait_for_write`).
     * If `WriteOptions::set_last_message()` is present then no more calls to `initiate_write` or `initiate_writes_done`
     * are allowed.
     */
    void initiate_write(const Request& request, grpc::WriteOptions options = {})
    {
        data_.write_.reset();
        this->StartWrite(&request, options);
    }

    /**
     * @brief Indicate that the rpc will have no more write operations
     *
     * This can only be issued once for a given rpc. This is not required or allowed if `initiate_write` with
     * `set_last_message()` is used since that already has the same implication. Note that calling this means that no
     * more calls to `initiate_write` or `initiate_writes_done` are allowed.
     */
    void initiate_writes_done()
    {
        remove_hold();
        this->StartWritesDone();
    }

    /**
     * @brief Wait for write
     *
     * Waits for the completion of a write. Only one wait for write may be outstanding at any time.
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
     * @brief Wait for writes done
     *
     * Waits for the completion of `writes_done`. Only one wait for write may be outstanding at any time.
     *
     * Completion signature is `void(error_code, bool)`. If the bool is `false` then the rpc failed (cancelled,
     * disconnected, deadline reached, ...).
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_writes_done(CompletionToken&& token = CompletionToken{})
    {
        return data_.writes_done_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

    /**
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. No more reads or writes may be initiated on
     * this rpc after this function has been called. Only one wait for finish may be outstanding at any time.
     *
     * Completion signature is `void(error_code, grpc::Status)`.
     */
    template <class CompletionToken = detail::DefaultCompletionTokenT<Executor>>
    auto wait_for_finish(CompletionToken&& token = CompletionToken{})
    {
        remove_hold();
        return data_.finish_.wait(static_cast<CompletionToken&&>(token), this->get_executor());
    }

  protected:
    BasicClientBidiReactor() = default;

  private:
    template <class>
    friend class detail::RefCountedReactorBase;

    void on_user_done() { remove_hold(); }

    void OnReadInitialMetadataDone(bool ok) final { data_.initial_metadata_.set(static_cast<bool&&>(ok)); }

    void OnReadDone(bool ok) final { data_.read_.set(static_cast<bool&&>(ok)); }

    void OnWriteDone(bool ok) final { data_.write_.set(static_cast<bool&&>(ok)); }

    void OnWritesDoneDone(bool ok) final { data_.writes_done_.set(static_cast<bool&&>(ok)); }

    void on_done(const grpc::Status& status) { data_.finish_.set(grpc::Status{status}); }

    void remove_hold()
    {
        if (!data_.is_hold_removed_.exchange(true, std::memory_order_relaxed))
        {
            this->RemoveHold();
        }
    }

    detail::ClientBidiReactorData data_;
};

/**
 * @brief (experimental) I/O object for client-side, bidi-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Request, class Response>
using ClientBidiReactor = BasicClientBidiReactor<Request, Response, asio::any_io_executor>;

template <class Request, class Response, class Executor>
using BasicClientBidiReactorBase =
    detail::RefCountedClientReactor<agrpc::BasicClientBidiReactor<Request, Response, Executor>>;

template <class Request, class Response>
using ClientBidiReactorBase = BasicClientBidiReactorBase<Request, Response, asio::any_io_executor>;

/**
 * @brief (experimental) Perform a unary rpc
 *
 * Completion signature is `void(error_code, grpc::Status)`. Once this operation completes the response passed to
 * it will have been be populated if `grpc::Status::ok()` is true.
 *
 * Example:
 *
 * @snippet client_callback.cpp client-rpc-unary-call
 *
 * Based on `.proto` file:
 *
 * @snippet example.proto example-proto
 *
 * **Per-Operation Cancellation**
 *
 * None (still in development)
 *
 * @since 3.5.0
 */
template <class StubAsync, class Request, class Response, class CompletionToken = detail::DefaultCompletionTokenT<void>>
auto unary_call(detail::AsyncUnaryFn<StubAsync, Request, Response> fn, StubAsync* stub,
                grpc::ClientContext& client_context, const Request& req, Response& response,
                CompletionToken&& token = CompletionToken{})
{
    return asio::async_initiate<CompletionToken, void(grpc::Status)>(
        [fn](auto handler, StubAsync* stub, grpc::ClientContext* client_context, const Request* req, Response* response)
        {
            (*stub.*fn)(client_context, req, response,
                        detail::UnaryRequestCallback{static_cast<decltype(handler)&&>(handler)});
        },
        token, stub, &client_context, &req, &response);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_CLIENT_CALLBACK_HPP
