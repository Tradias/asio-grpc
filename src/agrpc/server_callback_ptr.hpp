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

#ifndef AGRPC_AGRPC_SERVER_CALLBACK_PTR_HPP
#define AGRPC_AGRPC_SERVER_CALLBACK_PTR_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/reactor_ptr.hpp>
#include <agrpc/detail/reactor_ptr_type.hpp>
#include <agrpc/detail/server_callback_ptr.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

template <class Executor>
using BasicServerUnaryReactorBase = detail::RefCountedReactor<agrpc::BasicServerUnaryReactor<Executor>>;

using ServerUnaryReactorBase = BasicServerUnaryReactorBase<asio::any_io_executor>;

template <class Reactor>
class ReactorPtr
{
  private:
    using Allocation = detail::RefCountedReactorTypeT<Reactor>;
    using Ptr = Allocation*;

  public:
    using ValueType = Reactor;

    ReactorPtr() = default;

    ReactorPtr(const ReactorPtr& other) noexcept : ptr_(other.ptr_)
    {
        if (ptr_)
        {
            ptr_->increment_ref_count();
        }
    }

    ReactorPtr(ReactorPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    ~ReactorPtr() noexcept
    {
        if (ptr_)
        {
            ptr_->decrement_ref_count();
        }
    }

    ReactorPtr& operator=(const ReactorPtr& other) noexcept
    {
        if (this != &other)
        {
            if (ptr_)
            {
                ptr_->decrement_ref_count();
            }
            ptr_ = other.ptr_;
            if (ptr_)
            {
                ptr_->increment_ref_count();
            }
        }
        return *this;
    }

    ReactorPtr& operator=(ReactorPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr_)
            {
                ptr_->decrement_ref_count();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] bool operator==(const ReactorPtr& other) const noexcept { return ptr_ == other.ptr_; }

    [[nodiscard]] bool operator!=(const ReactorPtr& other) const noexcept { return !(*this == other); }

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

    [[nodiscard]] ValueType* operator->() const noexcept { return ptr_; }

    [[nodiscard]] ValueType& operator*() const noexcept { return *ptr_; }

  private:
    friend detail::ReactorPtrAccess;

    explicit ReactorPtr(Ptr ptr) noexcept : ptr_(ptr) {}

    Ptr ptr_{};
};

template <class Reactor, class Allocator, class... Args>
[[nodiscard]] inline ReactorPtr<Reactor> allocate_reactor(Allocator allocator, typename Reactor::executor_type executor,
                                                          Args&&... args)
{
    return detail::ReactorPtrAccess::create<ReactorPtr<Reactor>>(allocator, std::move(executor),
                                                                 static_cast<Args&&>(args)...);
}

template <class Reactor, class... Args>
[[nodiscard]] inline ReactorPtr<Reactor> make_reactor(typename Reactor::executor_type executor, Args&&... args)
{
    return agrpc::allocate_reactor<Reactor>(std::allocator<void>{}, std::move(executor), static_cast<Args&&>(args)...);
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_SERVER_CALLBACK_PTR_HPP
