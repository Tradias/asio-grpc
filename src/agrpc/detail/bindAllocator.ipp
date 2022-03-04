// Copyright 2022 Dennis Hezel
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

#ifndef AGRPC_DETAIL_BINDALLOCATOR_IPP
#define AGRPC_DETAIL_BINDALLOCATOR_IPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/forward.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class TargetAsyncResult, class Allocator, class = void>
struct AllocatorBinderAsyncResultCompletionHandlerType
{
};

template <class TargetAsyncResult, class Allocator>
struct AllocatorBinderAsyncResultCompletionHandlerType<TargetAsyncResult, Allocator,
                                                       std::void_t<typename TargetAsyncResult::completion_handler_type>>
{
    using completion_handler_type =
        agrpc::AllocatorBinder<typename TargetAsyncResult::completion_handler_type, Allocator>;
};

template <class TargetAsyncResult, class Allocator>
struct AllocatorBinderAsyncResultCompletionHandlerType<TargetAsyncResult, Allocator,
                                                       std::void_t<typename TargetAsyncResult::handler_type>>
{
    using completion_handler_type = agrpc::AllocatorBinder<typename TargetAsyncResult::handler_type, Allocator>;
};

template <class TargetAsyncResult, class = void>
struct AllocatorBinderAsyncResultReturnType
{
};

template <class TargetAsyncResult>
struct AllocatorBinderAsyncResultReturnType<TargetAsyncResult, std::void_t<typename TargetAsyncResult::return_type>>
{
    using return_type = typename TargetAsyncResult::return_type;
};

template <class Initiation, class Allocator>
struct AllocatorBinderAsyncResultInitWrapper
{
    template <class Init>
    AllocatorBinderAsyncResultInitWrapper(const Allocator& allocator, Init&& init)
        : allocator(allocator), initiation(std::forward<Init>(init))
    {
    }

    template <class Handler, class... Args>
    void operator()(Handler&& handler, Args&&... args)
    {
        std::move(initiation)(::agrpc::AllocatorBinder<::agrpc::detail::RemoveCvrefT<Handler>, Allocator>(
                                  allocator, std::forward<Handler>(handler)),
                              std::forward<Args>(args)...);
    }

    template <class Handler, class... Args>
    void operator()(Handler&& handler, Args&&... args) const
    {
        initiation(::agrpc::AllocatorBinder<::agrpc::detail::RemoveCvrefT<Handler>, Allocator>(
                       allocator, std::forward<Handler>(handler)),
                   std::forward<Args>(args)...);
    }

    Allocator allocator;
    Initiation initiation;
};
}  // namespace detail

AGRPC_NAMESPACE_END

AGRPC_ASIO_NAMESPACE_BEGIN()

template <class CompletionToken, class Allocator, class Signature>
class async_result<::agrpc::AllocatorBinder<CompletionToken, Allocator>, Signature>
    : public ::agrpc::detail::AllocatorBinderAsyncResultCompletionHandlerType<async_result<CompletionToken, Signature>,
                                                                              Allocator>,
      public ::agrpc::detail::AllocatorBinderAsyncResultReturnType<async_result<CompletionToken, Signature>>
{
  public:
    explicit async_result(::agrpc::AllocatorBinder<CompletionToken, Allocator>& binder) : result(binder.get()) {}

    decltype(auto) get() { return result.get(); }

    template <class Initiation, class RawCompletionToken, class... Args>
    static decltype(auto) initiate(Initiation&& initiation, RawCompletionToken&& token, Args&&... args)
    {
        return async_initiate<CompletionToken, Signature>(
            ::agrpc::detail::AllocatorBinderAsyncResultInitWrapper<::agrpc::detail::RemoveCvrefT<Initiation>,
                                                                   Allocator>(token.get_allocator(),
                                                                              std::forward<Initiation>(initiation)),
            token.get(), std::forward<Args>(args)...);
    }

  private:
    async_result<CompletionToken, Signature> result;
};

template <class Target, class Allocator, class Allocator1>
struct associated_allocator<::agrpc::AllocatorBinder<Target, Allocator>, Allocator1>
{
    using type = Allocator;

    static type get(const ::agrpc::AllocatorBinder<Target, Allocator>& b, const Allocator1& = Allocator1()) noexcept
    {
        return b.get_allocator();
    }
};

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT
template <template <class, class> class Associator, class Target, class Allocator, class DefaultCandidate>
struct associator<Associator, agrpc::AllocatorBinder<Target, Allocator>, DefaultCandidate>
{
    using type = typename Associator<Target, DefaultCandidate>::type;

    static type get(const agrpc::AllocatorBinder<Target, Allocator>& b,
                    const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<Target, DefaultCandidate>::get(b.get(), c);
    }
};
#endif

AGRPC_ASIO_NAMESPACE_END

#endif  // AGRPC_DETAIL_BINDALLOCATOR_IPP
