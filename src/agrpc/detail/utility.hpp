#ifndef AGRPC_DETAIL_UTILITY_HPP
#define AGRPC_DETAIL_UTILITY_HPP

#include <type_traits>
#include <utility>

namespace agrpc::detail
{
template <class First, class Second, bool = std::is_empty_v<Second> && !std::is_final_v<Second>>
class CompressedPair : private Second
{
  private:
    First first_;

  public:
    template <class T, class U>
    constexpr CompressedPair(T&& first, U&& second) noexcept(
        std::is_nothrow_constructible_v<First, T>&& std::is_nothrow_constructible_v<Second, U>)
        : Second(std::forward<U>(second)), first_(std::forward<T>(first))
    {
    }

    constexpr auto& first() noexcept { return this->first_; }

    constexpr const auto& first() const noexcept { return this->first_; }

    constexpr Second& second() noexcept { return *this; }

    constexpr const Second& second() const noexcept { return *this; }
};

template <class First, class Second>
class CompressedPair<First, Second, false> final
{
  private:
    First first_;
    Second second_;

  public:
    template <class T, class U>
    constexpr CompressedPair(T&& first, U&& second) noexcept(
        std::is_nothrow_constructible_v<First, T>&& std::is_nothrow_constructible_v<Second, U>)
        : first_(std::forward<T>(first)), second_(std::forward<U>(second))
    {
    }

    constexpr auto& first() noexcept { return this->first_; }

    constexpr const auto& first() const noexcept { return this->first_; }

    constexpr auto& second() noexcept { return this->second_; }

    constexpr const auto& second() const noexcept { return this->second_; }
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_UTILITY_HPP
