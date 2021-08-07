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

template <class F, class Args, std::size_t... I>
constexpr decltype(auto) invoke_front_impl(F&& f, Args&& args, std::index_sequence<I...>)
{
    if constexpr (std::is_invocable_v<F&&, std::tuple_element_t<I, Args>...>)
    {
        return std::invoke(std::forward<F>(f), std::get<I>(args)...);
    }
    else
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
