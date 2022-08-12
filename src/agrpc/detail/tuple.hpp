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

#ifndef AGRPC_DETAIL_TUPLE_HPP
#define AGRPC_DETAIL_TUPLE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>

#include <tuple>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class... T>
struct Tuple;

template <>
struct Tuple<>
{
};

template <class T0>
struct Tuple<T0>
{
    T0 v0;
};

template <class T0, class T1>
struct Tuple<T0, T1>
{
    T0 v0;
    T1 v1;
};

template <class T0, class T1, class T2>
struct Tuple<T0, T1, T2>
{
    T0 v0;
    T1 v1;
    T2 v2;
};

template <class T0, class T1, class T2, class... T>
struct Tuple<T0, T1, T2, T...>
{
    template <class Arg1, class Arg2, class... Args>
    Tuple(Arg1&& arg1, Arg2&& arg2, Args&&... args)
        : impl(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), std::forward<Args>(args)...)
    {
    }

    std::tuple<T0, T1, T2, T...> impl;
};

template <class... T>
Tuple(T...) -> Tuple<T...>;

template <class Tuple>
inline constexpr auto DECAY_TUPLE_SIZE = std::tuple_size_v<detail::RemoveCrefT<Tuple>>;

template <std::size_t I, class Tuple>
decltype(auto) get(Tuple&& tuple) noexcept
{
    static constexpr auto SIZE = detail::DECAY_TUPLE_SIZE<Tuple>;
    static_assert(I < SIZE, "index out of range");
    if constexpr (SIZE <= 3)
    {
        if constexpr (I == 0)
        {
            return (std::forward<Tuple>(tuple).v0);
        }
        else if constexpr (I == 1)
        {
            return (std::forward<Tuple>(tuple).v1);
        }
        else if constexpr (I == 2)
        {
            return (std::forward<Tuple>(tuple).v2);
        }
    }
    else
    {
        return std::get<I>(std::forward<Tuple>(tuple).impl);
    }
}

template <class F, class Tuple, std::size_t... I>
decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>)
{
    return std::forward<F>(f)(detail::get<I>(std::forward<Tuple>(t))...);
}

template <class F, class Tuple>
decltype(auto) apply(F&& f, Tuple&& t)
{
    static constexpr auto SIZE = detail::DECAY_TUPLE_SIZE<Tuple>;
    if constexpr (SIZE <= 3)
    {
        return detail::apply_impl(std::forward<F>(f), std::forward<Tuple>(t), std::make_index_sequence<SIZE>{});
    }
    else
    {
        return std::apply(std::forward<F>(f), std::forward<Tuple>(t).impl);
    }
}

template <class Arg, class Tuple, std::size_t... I>
auto prepend_to_tuple_impl(Arg&& arg, Tuple&& t, std::index_sequence<I...>)
{
    return detail::Tuple{std::forward<Arg>(arg), detail::get<I>(std::forward<Tuple>(t))...};
}

template <class Arg, class Tuple>
auto prepend_to_tuple(Arg&& arg, Tuple&& t)
{
    return detail::prepend_to_tuple_impl(std::forward<Arg>(arg), std::forward<Tuple>(t),
                                         std::make_index_sequence<detail::DECAY_TUPLE_SIZE<Tuple>>{});
}
}

AGRPC_NAMESPACE_END

template <class... T>
struct std::tuple_size<agrpc::detail::Tuple<T...>>
{
    static constexpr auto value = sizeof...(T);
};

#endif  // AGRPC_DETAIL_TUPLE_HPP
