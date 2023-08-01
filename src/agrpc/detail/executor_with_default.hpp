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

#ifndef AGRPC_DETAIL_EXECUTOR_WITH_DEFAULT_HPP
#define AGRPC_DETAIL_EXECUTOR_WITH_DEFAULT_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Default, class Executor>
struct ExecutorWithDefault : Executor
{
    using default_completion_token_type = Default;

    template <class Executor1, class = std::enable_if_t<(!std::is_same_v<Executor1, ExecutorWithDefault> &&
                                                         std::is_convertible_v<Executor1, Executor>)>>
    ExecutorWithDefault(const Executor1& ex) noexcept : Executor(ex)
    {
    }
};
}

AGRPC_NAMESPACE_END

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)
template <class Default, class Executor>
struct agrpc::asio::traits::equality_comparable<agrpc::detail::ExecutorWithDefault<Default, Executor>>
    : agrpc::asio::traits::equality_comparable<Executor>
{
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)
template <class Default, class Executor, class F>
struct agrpc::asio::traits::execute_member<agrpc::detail::ExecutorWithDefault<Default, Executor>, F>
    : agrpc::asio::traits::execute_member<Executor, F>
{
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)
template <class Default, class Executor>
struct agrpc::asio::traits::require_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                           agrpc::asio::execution::blocking_t::possibly_t>
    : agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::blocking_t::possibly_t>
{
    using result_type =
        agrpc::detail::ExecutorWithDefault<Default,
                                           typename agrpc::asio::traits::require_member<
                                               Executor, agrpc::asio::execution::blocking_t::possibly_t>::result_type>;
};

template <class Default, class Executor>
struct agrpc::asio::traits::require_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                           agrpc::asio::execution::blocking_t::never_t>
    : agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::blocking_t::never_t>
{
    using result_type =
        agrpc::detail::ExecutorWithDefault<Default,
                                           typename agrpc::asio::traits::require_member<
                                               Executor, agrpc::asio::execution::blocking_t::never_t>::result_type>;
};

template <class Default, class Executor>
struct agrpc::asio::traits::require_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                           agrpc::asio::execution::outstanding_work_t::tracked_t>
    : agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::outstanding_work_t::tracked_t>
{
    using result_type = agrpc::detail::ExecutorWithDefault<
        Default, typename agrpc::asio::traits::require_member<
                     Executor, agrpc::asio::execution::outstanding_work_t::tracked_t>::result_type>;
};

template <class Default, class Executor>
struct agrpc::asio::traits::require_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                           agrpc::asio::execution::outstanding_work_t::untracked_t>
    : agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::outstanding_work_t::untracked_t>
{
    using result_type = agrpc::detail::ExecutorWithDefault<
        Default, typename agrpc::asio::traits::require_member<
                     Executor, agrpc::asio::execution::outstanding_work_t::untracked_t>::result_type>;
};

template <class Default, class Executor>
struct agrpc::asio::traits::require_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                           agrpc::asio::execution::allocator_t<void>>
    : agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::allocator_t<void>>
{
    using result_type = agrpc::detail::ExecutorWithDefault<
        Default,
        typename agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::allocator_t<void>>::result_type>;
};

template <class Default, class Executor, typename OtherAllocator>
struct agrpc::asio::traits::require_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                           agrpc::asio::execution::allocator_t<OtherAllocator>>
    : agrpc::asio::traits::require_member<Executor, agrpc::asio::execution::allocator_t<OtherAllocator>>
{
    using result_type = agrpc::detail::ExecutorWithDefault<
        Default, typename agrpc::asio::traits::require_member<
                     Executor, agrpc::asio::execution::allocator_t<OtherAllocator>>::result_type>;
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT)
template <class Default, class Executor>
struct agrpc::asio::traits::prefer_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                          agrpc::asio::execution::relationship_t::fork_t>
    : agrpc::asio::traits::prefer_member<Executor, agrpc::asio::execution::relationship_t::fork_t>
{
    using result_type =
        agrpc::detail::ExecutorWithDefault<Default,
                                           typename agrpc::asio::traits::prefer_member<
                                               Executor, agrpc::asio::execution::relationship_t::fork_t>::result_type>;
};

template <class Default, class Executor>
struct agrpc::asio::traits::prefer_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                          agrpc::asio::execution::relationship_t::continuation_t>
    : agrpc::asio::traits::prefer_member<Executor, agrpc::asio::execution::relationship_t::continuation_t>
{
    using result_type = agrpc::detail::ExecutorWithDefault<
        Default, typename agrpc::asio::traits::prefer_member<
                     Executor, agrpc::asio::execution::relationship_t::continuation_t>::result_type>;
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)
template <class Default, class Executor, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::detail::ExecutorWithDefault<Default, Executor>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::blocking_t>>>
    : agrpc::asio::traits::query_static_constexpr_member<Executor, Property>
{
    static constexpr auto value()
    {
        auto executor = Executor::query(Property{});
        return agrpc::detail::ExecutorWithDefault<Default, decltype(executor)>(executor);
    }

    using result_type = decltype(value());
};

template <class Default, class Executor, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::detail::ExecutorWithDefault<Default, Executor>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::relationship_t>>>
    : agrpc::asio::traits::query_static_constexpr_member<Executor, Property>
{
    static constexpr auto value()
    {
        auto executor = Executor::query(Property{});
        return agrpc::detail::ExecutorWithDefault<Default, decltype(executor)>(executor);
    }

    using result_type = decltype(value());
};

template <class Default, class Executor, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::detail::ExecutorWithDefault<Default, Executor>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::outstanding_work_t>>>
    : agrpc::asio::traits::query_static_constexpr_member<Executor, Property>
{
    static constexpr auto value()
    {
        auto executor = Executor::query(Property{});
        return agrpc::detail::ExecutorWithDefault<Default, decltype(executor)>(executor);
    }

    using result_type = decltype(value());
};

template <class Default, class Executor, class Property>
struct agrpc::asio::traits::query_static_constexpr_member<
    agrpc::detail::ExecutorWithDefault<Default, Executor>, Property,
    typename std::enable_if_t<std::is_convertible_v<Property, agrpc::asio::execution::mapping_t>>>
    : agrpc::asio::traits::query_static_constexpr_member<Executor, Property>
{
    static constexpr auto value()
    {
        auto executor = Executor::query(Property{});
        return agrpc::detail::ExecutorWithDefault<Default, decltype(executor)>(executor);
    }
};
#endif

#if !defined(AGRPC_UNIFEX) && !defined(BOOST_ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT) && \
    !defined(ASIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)
template <class Default, class Executor>
struct agrpc::asio::traits::query_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                         agrpc::asio::execution::context_t>
    : agrpc::asio::traits::query_member<Executor, agrpc::asio::execution::context_t>
{
};

template <class Default, class Executor, class OtherAllocator>
struct agrpc::asio::traits::query_member<agrpc::detail::ExecutorWithDefault<Default, Executor>,
                                         agrpc::asio::execution::allocator_t<OtherAllocator>>
    : agrpc::asio::traits::query_member<Executor, agrpc::asio::execution::allocator_t<OtherAllocator>>
{
};
#endif

#endif  // AGRPC_DETAIL_EXECUTOR_WITH_DEFAULT_HPP
