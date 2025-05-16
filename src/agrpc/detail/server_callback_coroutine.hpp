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

#ifndef AGRPC_DETAIL_SERVER_CALLBACK_COROUTINE_HPP
#define AGRPC_DETAIL_SERVER_CALLBACK_COROUTINE_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/reactor_ptr_type.hpp>
#include <agrpc/detail/ref_counted_reactor.hpp>
#include <agrpc/server_callback.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/this_coro.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct GetReactorArg
{
};

struct InitiateFinishArg
{
    grpc::Status status_;
};

struct WaitForFinishArg
{
};

struct InitiateSendInitialMetadataArg
{
};

struct WaitForSendInitialMetadataArg
{
};

template <class Request>
struct InitiateReadArg
{
    Request& request_;
};

struct WaitForReadArg
{
};

template <class Reactor>
class ServerReactorPromiseBase
{
  private:
    using R = detail::RefCountedReactorTypeT<Reactor>;

  protected:
    ~ServerReactorPromiseBase() {}

    R& reactor() noexcept { return reactor_; }

    void destruct_reactor() { reactor_.~R(); }

  private:
    union
    {
        R reactor_{};
    };
};

template <class Reactor>
class ServerReactorPromiseType final : private detail::ServerReactorPromiseBase<Reactor>,
                                       public boost::cobalt::enable_await_deferred,
                                       public boost::cobalt::enable_await_executor<ServerReactorPromiseType<Reactor>>
{
  private:
    using Base = detail::ServerReactorPromiseBase<Reactor>;
    using Handle = std::coroutine_handle<ServerReactorPromiseType>;

  public:
    using executor_type = typename Reactor::executor_type;

    static void* operator new(std::size_t size) { return ::operator new(size); }

    static void operator delete(void* ptr) noexcept
    {
        auto& self = Handle::from_address(ptr).promise();
        self.reactor().decrement_ref_count();
    }

    // TODO Clang does not pass `this` as first argument
    template <class Service, class... Args>
    ServerReactorPromiseType(Service&& service, Args&&...)
    {
        detail::ReactorAccess::set_executor(reactor(), asio::get_associated_executor(service));
        reactor().set_deallocate_function(&deallocate);
    }

    auto* get_return_object() noexcept { return reactor().get(); }

    std::suspend_never initial_suspend() const noexcept { return {}; }

    void return_void() const noexcept {}

    void unhandled_exception() noexcept { finish({grpc::StatusCode::INTERNAL, "Unhandled exception"}); }

    std::suspend_never final_suspend() const noexcept { return {}; }

    decltype(auto) get_executor() noexcept { return reactor().get_executor(); }

    using boost::cobalt::enable_await_deferred::await_transform;

    using boost::cobalt::enable_await_executor<ServerReactorPromiseType>::await_transform;

    auto await_transform(detail::GetReactorArg) noexcept
    {
        struct Awaitable
        {
            bool await_ready() const noexcept { return true; }
            void await_suspend(std::coroutine_handle<>) const noexcept {}
            Reactor& await_resume() const noexcept { return reactor_; }

            Reactor& reactor_;
        };
        return Awaitable{reactor()};
    }

    std::suspend_never await_transform(detail::InitiateSendInitialMetadataArg) noexcept
    {
        reactor().initiate_send_initial_metadata();
        return {};
    }

    auto await_transform(detail::WaitForSendInitialMetadataArg) noexcept
    {
        return reactor().wait_for_send_initial_metadata(boost::cobalt::use_op);
    }

    template <class Request>
    std::suspend_never await_transform(detail::InitiateReadArg<Request> arg) noexcept
    {
        reactor().initiate_read(arg.request_);
        return {};
    }

    auto await_transform(detail::WaitForReadArg) noexcept { return reactor().wait_for_read(boost::cobalt::use_op); }

    std::suspend_never await_transform(detail::InitiateFinishArg arg) noexcept
    {
        finish(std::move(arg.status_));
        return {};
    }

    auto await_transform(detail::WaitForFinishArg) noexcept { return reactor().wait_for_finish(boost::cobalt::use_op); }

    template <class Arg>
    Arg&& await_transform(Arg&& arg) noexcept
    {
        return static_cast<Arg&&>(arg);
    }

  private:
    using Base::reactor;

    void finish(grpc::Status&& status) { reactor().initiate_finish(static_cast<grpc::Status&&>(status)); }

    static void deallocate(void* ptr) noexcept
    {
        auto& self = *static_cast<ServerReactorPromiseType*>(ptr);
        detail::ReactorAccess::destroy_executor(self.reactor());
        self.destruct_reactor();
        ::operator delete(Handle::from_promise(self).address());
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SERVER_CALLBACK_COROUTINE_HPP
