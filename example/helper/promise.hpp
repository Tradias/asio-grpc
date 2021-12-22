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

#ifndef AGRPC_HELPER_PROMISE_HPP
#define AGRPC_HELPER_PROMISE_HPP

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/post.hpp>

#include <optional>

namespace detail
{
template <class Signature, class CompletionToken, class = void>
struct CompletionHandlerType
{
    using Type = typename boost::asio::async_result<CompletionToken, Signature>::completion_handler_type;
};

template <class Signature, class CompletionToken>
struct CompletionHandlerType<Signature, CompletionToken,
                             std::void_t<typename boost::asio::async_result<CompletionToken, Signature>::handler_type>>
{
    using Type = typename boost::asio::async_result<CompletionToken, Signature>::handler_type;
};

template <class Signature, class CompletionToken>
using CompletionHandlerTypeT = typename CompletionHandlerType<Signature, CompletionToken>::Type;
}

template <class T, class CompletionToken>
struct BasicPromise
{
  public:
    auto get(CompletionToken token = {})
    {
        return boost::asio::async_initiate<CompletionToken, void(T)>(
            [&](auto&& completion_handler)
            {
                if (value)
                {
                    const auto executor = boost::asio::get_associated_executor(completion_handler);
                    boost::asio::post(executor,
                                      [extracted_value = extract_value(),
                                       completion_handler = std::move(completion_handler)]() mutable
                                      {
                                          completion_handler(std::move(extracted_value));
                                      });
                }
                else
                {
                    this->completion_handler.emplace(std::move(completion_handler));
                }
            },
            token);
    }

    template <class... Args>
    void fulfill(Args&&... args)
    {
        if (this->completion_handler)
        {
            const auto executor = boost::asio::get_associated_executor(*this->completion_handler);
            boost::asio::post(executor,
                              [completion_handler = this->extract_completion_handler(),
                               result = T{std::forward<Args>(args)...}]() mutable
                              {
                                  completion_handler(std::move(result));
                              });
        }
        else
        {
            value.emplace(std::forward<Args>(args)...);
        }
    }

    constexpr bool is_fulfilled() const noexcept { return value.has_value(); }

    void reset() noexcept { value.reset(); }

  private:
    using CompletionHandler = detail::CompletionHandlerTypeT<void(T), CompletionToken>;

    std::optional<CompletionHandler> completion_handler;
    std::optional<T> value;

    auto extract_completion_handler()
    {
        auto ch = std::move(*completion_handler);
        completion_handler.reset();
        return ch;
    }

    auto extract_value()
    {
        auto extracted_value = std::move(*value);
        value.reset();
        return extracted_value;
    }
};

#endif  // AGRPC_HELPER_PROMISE_HPP
