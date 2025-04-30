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

#ifndef AGRPC_DETAIL_REF_COUNTED_REACTOR_HPP
#define AGRPC_DETAIL_REF_COUNTED_REACTOR_HPP

#include <agrpc/detail/forward.hpp>
#include <agrpc/detail/reactor_ptr.hpp>
#include <grpcpp/support/status.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Reactor>
class RefCountedServerReactor;

template <class Reactor>
class RefCountedReactorBase : public Reactor
{
  public:
    struct InitArg
    {
        typename Reactor::executor_type executor_;
        ReactorDeallocateFn deallocate_;
    };

    explicit RefCountedReactorBase(InitArg init_arg) noexcept
        : Reactor(static_cast<typename Reactor::executor_type&&>(init_arg.executor_)), deallocate_(init_arg.deallocate_)
    {
    }

  private:
    template <class>
    friend class agrpc::ReactorPtr;

    template <class>
    friend class detail::RefCountedServerReactor;

    template <class>
    friend class detail::RefCountedClientReactor;

    struct Guard
    {
        ~Guard() { self_.decrement_ref_count(); }

        RefCountedReactorBase& self_;
    };

    void increment_ref_count() noexcept { ++ref_count_; }

    void decrement_ref_count()
    {
        const auto count = --ref_count_;
        if (1 == count)
        {
            this->on_user_done();
        }
        else if (0 == count)
        {
            deallocate_(this);
        }
    }

    std::atomic_size_t ref_count_{2};  // user + OnDone
    ReactorDeallocateFn deallocate_;
};

template <class Reactor>
class RefCountedServerReactor : public RefCountedReactorBase<Reactor>
{
  public:
    using RefCountedServerReactor::RefCountedReactorBase::RefCountedReactorBase;

  private:
    void OnDone() final
    {
        typename RefCountedServerReactor::Guard g{*this};
        this->on_done();
    }
};

template <class Reactor>
class RefCountedClientReactor : public RefCountedReactorBase<Reactor>
{
  public:
    using RefCountedClientReactor::RefCountedReactorBase::RefCountedReactorBase;

  private:
    void OnDone(const grpc::Status& status) final
    {
        typename RefCountedClientReactor::Guard g{*this};
        this->on_done(status);
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_REF_COUNTED_REACTOR_HPP
