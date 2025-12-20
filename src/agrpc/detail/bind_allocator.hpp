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

#ifndef AGRPC_DETAIL_BIND_ALLOCATOR_HPP
#define AGRPC_DETAIL_BIND_ALLOCATOR_HPP

#include <agrpc/detail/association.hpp>
#include <agrpc/detail/utility.hpp>

#include <memory>

#include <agrpc/detail/asio_macros.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Target, class Allocator>
class AllocatorBinder
{
  public:
    using target_type = Target;
    using allocator_type = Allocator;

    template <class... Args>
    constexpr explicit AllocatorBinder(const Allocator& allocator, Args&&... args)
        : impl_(detail::SecondThenVariadic{}, allocator, static_cast<Args&&>(args)...)
    {
    }

    AllocatorBinder(const AllocatorBinder& other) = default;

    template <class OtherTarget, class OtherAllocator>
    constexpr explicit AllocatorBinder(const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl_(other.get(), other.impl_.second())
    {
    }

    template <class OtherTarget, class OtherAllocator>
    constexpr AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl_(other.get(), allocator)
    {
    }

    template <class OtherTarget, class OtherAllocator>
    constexpr AllocatorBinder(const Allocator& allocator, const AllocatorBinder<OtherTarget, OtherAllocator>& other)
        : impl_(other.get(), allocator)
    {
    }

    AllocatorBinder(AllocatorBinder&& other) = default;

    template <class OtherTarget, class OtherAllocator>
    constexpr explicit AllocatorBinder(AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : impl_(static_cast<OtherTarget&&>(other.get()), static_cast<OtherAllocator&&>(other.impl_.second()))
    {
    }

    template <class OtherTarget, class OtherAllocator>
    constexpr AllocatorBinder(const Allocator& allocator, AllocatorBinder<OtherTarget, OtherAllocator>&& other)
        : impl_(static_cast<OtherTarget&&>(other.get()), allocator)
    {
    }

    ~AllocatorBinder() = default;
    AllocatorBinder& operator=(const AllocatorBinder& other) = default;
    AllocatorBinder& operator=(AllocatorBinder&& other) = default;

    constexpr target_type& get() noexcept { return impl_.first(); }

    constexpr const target_type& get() const noexcept { return impl_.first(); }

    constexpr Allocator get_allocator() const noexcept { return impl_.second(); }

#if defined(AGRPC_UNIFEX) || defined(AGRPC_STDEXEC)
    friend Allocator tag_invoke(detail::exec::get_allocator_t, const AllocatorBinder& binder) noexcept
    {
        return binder.get_allocator();
    }
#endif

    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) &&
    {
        return static_cast<Target&&>(get())(static_cast<Args&&>(args)...);
    }

    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) &
    {
        return get()(static_cast<Args&&>(args)...);
    }

    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) const&
    {
        return get()(static_cast<Args&&>(args)...);
    }

  private:
    template <class, class>
    friend class detail::AllocatorBinder;

    detail::CompressedPair<Target, Allocator> impl_;
};

template <class Allocator, class Target>
AllocatorBinder(const Allocator& allocator, Target&& target) -> AllocatorBinder<detail::RemoveCrefT<Target>, Allocator>;

// Implementation details
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

template <class TargetAsyncResult, class Allocator, class = void>
struct AllocatorBinderAsyncResultCompletionHandlerType
{
};

template <class TargetAsyncResult, class Allocator>
struct AllocatorBinderAsyncResultCompletionHandlerType<TargetAsyncResult, Allocator,
                                                       std::void_t<typename TargetAsyncResult::completion_handler_type>>
{
    using completion_handler_type =
        detail::AllocatorBinder<typename TargetAsyncResult::completion_handler_type, Allocator>;
};

template <class TargetAsyncResult, class Allocator, class = void>
struct AllocatorBinderAsyncResultHandlerType
{
};

template <class TargetAsyncResult, class Allocator>
struct AllocatorBinderAsyncResultHandlerType<TargetAsyncResult, Allocator,
                                             std::void_t<typename TargetAsyncResult::handler_type>>
{
    using handler_type = detail::AllocatorBinder<typename TargetAsyncResult::handler_type, Allocator>;
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
    template <class Handler, class... Args>
    constexpr void operator()(Handler&& handler, Args&&... args) &&
    {
        static_cast<Initiation&&>(initiation_)(detail::AllocatorBinder(allocator_, static_cast<Handler&&>(handler)),
                                               static_cast<Args&&>(args)...);
    }

    Allocator allocator_;
    Initiation initiation_;
};
#endif
}  // namespace detail

AGRPC_NAMESPACE_END

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

template <class CompletionToken, class Allocator, class Signature>
class agrpc::asio::async_result<agrpc::detail::AllocatorBinder<CompletionToken, Allocator>, Signature>
    : public agrpc::detail::AllocatorBinderAsyncResultCompletionHandlerType<async_result<CompletionToken, Signature>,
                                                                            Allocator>,
      public agrpc::detail::AllocatorBinderAsyncResultHandlerType<async_result<CompletionToken, Signature>, Allocator>,
      public agrpc::detail::AllocatorBinderAsyncResultReturnType<async_result<CompletionToken, Signature>>
{
  public:
    constexpr explicit async_result(agrpc::detail::AllocatorBinder<CompletionToken, Allocator>& binder)
        : result_(binder.get())
    {
    }

    constexpr decltype(auto) get() { return result_.get(); }

    template <class Initiation, class BoundCompletionToken, class... Args>
    static decltype(auto) initiate(Initiation&& initiation, BoundCompletionToken&& token, Args&&... args)
    {
        return asio::async_initiate<CompletionToken, Signature>(
            agrpc::detail::AllocatorBinderAsyncResultInitWrapper<agrpc::detail::RemoveCrefT<Initiation>, Allocator>{
                token.get_allocator(), static_cast<Initiation&&>(initiation)},
            static_cast<BoundCompletionToken&&>(token).get(), static_cast<Args&&>(args)...);
    }

  private:
    async_result<CompletionToken, Signature> result_;
};

template <class Target, class Allocator, class Allocator1>
struct agrpc::asio::associated_allocator<agrpc::detail::AllocatorBinder<Target, Allocator>, Allocator1>
{
    using type = Allocator;

    static constexpr decltype(auto) get(const agrpc::detail::AllocatorBinder<Target, Allocator>& b,
                                        const Allocator1& = Allocator1()) noexcept
    {
        return b.get_allocator();
    }
};

template <class Target, class Allocator, class DefaultCandidate>
struct agrpc::asio::associated_executor<agrpc::detail::AllocatorBinder<Target, Allocator>, DefaultCandidate>
{
    using type = asio::associated_executor_t<Target, DefaultCandidate>;

    static decltype(auto) get(const agrpc::detail::AllocatorBinder<Target, Allocator>& b,
                              const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return asio::get_associated_executor(b.get(), c);
    }
};

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <template <class, class> class Associator, class Target, class Allocator, class DefaultCandidate>
struct agrpc::asio::associator<Associator, agrpc::detail::AllocatorBinder<Target, Allocator>, DefaultCandidate>
{
    using type = typename Associator<Target, DefaultCandidate>::type;

    static constexpr decltype(auto) get(const agrpc::detail::AllocatorBinder<Target, Allocator>& b,
                                        const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<Target, DefaultCandidate>::get(b.get(), c);
    }
};

#endif

#endif

template <class Allocator, class Target, class Alloc>
struct std::uses_allocator<agrpc::detail::AllocatorBinder<Allocator, Target>, Alloc> : std::false_type
{
};

#endif  // AGRPC_DETAIL_BIND_ALLOCATOR_HPP
