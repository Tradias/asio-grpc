// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_COMPLETION_HANDLER_RECEIVER_HPP
#define AGRPC_DETAIL_COMPLETION_HANDLER_RECEIVER_HPP

#include <agrpc/detail/asio_association.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/memory_resource.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class>
struct InvokeSuccessfully
{
    template <class Ch, class... Args>
    static void invoke(Ch&& ch, Args&&... args)
    {
        static_cast<Ch&&>(ch)(static_cast<Args&&>(args)...);
    }
};

template <class... Args>
struct InvokeSuccessfully<void(detail::ErrorCode, Args...)>
{
    template <class Ch, class... T>
    static void invoke(Ch&& ch, T&&... args)
    {
        static_cast<Ch&&>(ch)(detail::ErrorCode{}, static_cast<T&&>(args)...);
    }
};

template <class Signature>
struct InvokeCancelled
{
    template <class Ch>
    static constexpr void invoke(Ch&&) noexcept
    {
    }
};

template <class... Args>
struct InvokeCancelled<void(detail::ErrorCode, Args...)>
{
    template <class Ch>
    static void invoke(Ch&& ch)
    {
        static_cast<Ch&&>(ch)(detail::operation_aborted_error_code(), Args{}...);
    }
};

template <class CompletionHandler, class Signature = void>
class CompletionHandlerReceiver
{
  public:
    using allocator_type = detail::AssociatedAllocatorT<CompletionHandler>;

    template <class Ch>
    explicit CompletionHandlerReceiver(Ch&& ch) : completion_handler_(static_cast<Ch&&>(ch))
    {
    }

    void set_done() { InvokeCancelled<Signature>::invoke(static_cast<CompletionHandler&&>(completion_handler_)); }

    template <class... Args>
    void set_value(Args&&... args) &&
    {
        InvokeSuccessfully<Signature>::invoke(static_cast<CompletionHandler&&>(completion_handler_),
                                              static_cast<Args&&>(args)...);
    }

    static void set_error(const std::exception_ptr& ep) { std::rethrow_exception(ep); }

    allocator_type get_allocator() const noexcept { return detail::exec::get_allocator(completion_handler_); }

    const CompletionHandler& completion_handler() const noexcept { return completion_handler_; }

  private:
    CompletionHandler completion_handler_;
};
}

AGRPC_NAMESPACE_END

template <class CompletionHandler, class Signature, class Alloc>
struct agrpc::detail::container::uses_allocator<agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>,
                                                Alloc> : std::false_type
{
};

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)

template <class CompletionHandler, class Signature, class Allocator1>
struct agrpc::asio::associated_allocator<agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>,
                                         Allocator1>
{
    using type = asio::associated_allocator_t<CompletionHandler, Allocator1>;

    static constexpr decltype(auto) get(const agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>& b,
                                        const Allocator1& allocator = Allocator1()) noexcept
    {
        return asio::get_associated_allocator(b.completion_handler(), allocator);
    }
};

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

template <template <class, class> class Associator, class CompletionHandler, class Signature, class DefaultCandidate>
struct agrpc::asio::associator<Associator, agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>,
                               DefaultCandidate>
{
    using type = typename Associator<CompletionHandler, DefaultCandidate>::type;

    static constexpr decltype(auto) get(const agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>& b,
                                        const DefaultCandidate& c = DefaultCandidate()) noexcept
    {
        return Associator<CompletionHandler, DefaultCandidate>::get(b.completion_handler(), c);
    }
};

#endif

#endif

#ifdef AGRPC_ASIO_HAS_SENDER_RECEIVER
#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT) && !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
template <class CompletionHandler, class Signature>
struct agrpc::asio::traits::set_done_member<agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = void;
};
#endif

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT) && !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)
template <class CompletionHandler, class Signature, class Vs>
struct agrpc::asio::traits::set_value_member<agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>, Vs>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
};
#endif

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT) && !defined(ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)
template <class CompletionHandler, class Signature, class E>
struct agrpc::asio::traits::set_error_member<agrpc::detail::CompletionHandlerReceiver<CompletionHandler, Signature>, E>
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = false;

    using result_type = void;
};
#endif
#endif

#endif  // AGRPC_DETAIL_COMPLETION_HANDLER_RECEIVER_HPP
