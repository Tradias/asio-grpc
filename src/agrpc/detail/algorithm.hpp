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

#ifndef AGRPC_DETAIL_ALGORITHM_HPP
#define AGRPC_DETAIL_ALGORITHM_HPP

#include <agrpc/detail/config.hpp>

#include <iterator>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class ForwardIt1, class ForwardIt2>
constexpr ForwardIt1 search(ForwardIt1 first, ForwardIt1 last, ForwardIt2 s_first, ForwardIt2 s_last)
{
    while (true)
    {
        ForwardIt1 it = first;
        for (ForwardIt2 s_it = s_first;; (void)++it, (void)++s_it)
        {
            if (s_it == s_last)
            {
                return first;
            }
            if (it == last)
            {
                return last;
            }
            if (!(*it == *s_it))
            {
                break;
            }
        }
        ++first;
    }
}

template <class InputIt, class T>
constexpr InputIt find(InputIt first, InputIt last, const T& value)
{
    for (; first != last; ++first)
    {
        if (*first == value)
        {
            return first;
        }
    }
    return last;
}

template <class InputIt, class OutputIt>
constexpr OutputIt copy(InputIt first, InputIt last, OutputIt d_first)
{
    for (; first != last; (void)++first, (void)++d_first)
    {
        *d_first = *first;
    }
    return d_first;
}

template <class InputIt, class OutputIt>
constexpr OutputIt move(InputIt first, InputIt last, OutputIt d_first)
{
    while (first != last)
    {
        *d_first++ = std::move(*first++);
    }
    return d_first;
}

template <class InputIt, class SearchRange, class Value>
constexpr auto replace_sequence_with_value(InputIt it, InputIt last, const SearchRange& search_range,
                                           const Value& replacement)
{
    const auto elements_to_be_removed = std::distance(std::begin(search_range), std::end(search_range)) - 1;
    while (true)
    {
        auto seq_start = detail::search(it, last, std::begin(search_range), std::end(search_range));
        if (seq_start == last)
        {
            break;
        }
        *seq_start = replacement;
        it = seq_start;
        ++it;
        last = detail::move(std::next(it, elements_to_be_removed), last, it);
    }
    return last;
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_ALGORITHM_HPP
