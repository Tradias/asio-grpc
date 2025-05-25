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
 * @brief I/O object for client-side, unary rpcs
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
 * @brief I/O object for client-side, unary rpcs (specialized on `asio::any_io_executor`)
 */
using ClientUnaryReactor = BasicClientUnaryReactor<asio::any_io_executor>;

template <class Executor>
using BasicClientUnaryReactorBase = detail::RefCountedClientReactor<agrpc::BasicClientUnaryReactor<Executor>>;

using ClientUnaryReactorBase = BasicClientUnaryReactorBase<asio::any_io_executor>;

/**
 * @brief I/O object for client-side, client-streaming rpcs
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
        (*stub.*fn)(&this->context(), &response, static_cast<BasicClientWriteReactor*>(this));
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
     * Initiate the write of a message. The argument must remain valid until the write completes (use `wait_for_write`).
     */
    void initiate_write(const Request& request)
    {
        data_.write_.reset();
        this->StartWrite(&request);
    }

    /**
     * @brief Wait for write
     *
     * Waits for the completion of `initiate_write` or `initiate_write_last`. Only one wait for write may be outstanding
     * at any time.
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
     * @brief Wait for finish
     *
     * Wait until all operations associated with this rpc have completed. No more writes may be initiated on thie rpc
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
 * @brief I/O object for client-side, client-streaming rpcs (specialized on `asio::any_io_executor`)
 */
template <class Request>
using ClientWriteReactor = BasicClientWriteReactor<Request, asio::any_io_executor>;

template <class StubAsync, class Request, class Response, class CompletionToken>
auto request(detail::AsyncUnaryFn<StubAsync, Request, Response> fn, StubAsync* stub,
             grpc::ClientContext& client_context, const Request& request, Response& response, CompletionToken&& token)
{
    return asio::async_initiate<CompletionToken, void(grpc::Status)>(
        [fn](auto handler, StubAsync* stub, grpc::ClientContext* client_context, const Request* request,
             Response* response)
        {
            (*stub.*fn)(client_context, request, response,
                        detail::UnaryRequestCallback{static_cast<decltype(handler)&&>(handler)});
        },
        token, stub, &client_context, &request, &response);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_CLIENT_CALLBACK_HPP
