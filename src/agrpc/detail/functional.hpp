// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_FUNCTIONAL_HPP
#define AGRPC_DETAIL_FUNCTIONAL_HPP

#include <boost/type_traits/remove_cv_ref.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace agrpc::detail
{
template <class T>
struct Always
{
    T t;

    template <class... Args>
    constexpr auto operator()(Args&&...) const noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        return t;
    }
};

template <class T>
Always(T&&) -> Always<boost::remove_cv_ref_t<T>>;

template <class F, class... Args, std::size_t... I>
constexpr decltype(auto) invoke_front_impl(F&& f, std::tuple<Args...>&& args, std::index_sequence<I...>)
{
    if constexpr (std::is_invocable_v<F&&, std::tuple_element_t<I, std::tuple<Args...>>...>)
    {
        return std::invoke(std::forward<F>(f), std::get<I>(std::move(args))...);
    }
    else if constexpr (sizeof...(I) > 0)
    {
        return detail::invoke_front_impl(std::forward<F>(f), std::move(args),
                                         std::make_index_sequence<sizeof...(I) - 1>());
    }
}

template <class F, class... Args>
constexpr decltype(auto) invoke_front(F&& f, Args&&... args)
{
    return detail::invoke_front_impl(std::forward<F>(f), std::forward_as_tuple(std::forward<Args>(args)...),
                                     std::make_index_sequence<sizeof...(Args)>());
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_FUNCTIONAL_HPP
