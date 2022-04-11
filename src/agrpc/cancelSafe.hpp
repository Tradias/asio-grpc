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

#ifndef AGRPC_AGRPC_CANCELSAFE_HPP
#define AGRPC_AGRPC_CANCELSAFE_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

#ifdef AGRPC_ASIO_HAS_CANCELLATION_SLOT

#include "agrpc/detail/typeErasedCompletionHandler.hpp"
#include "agrpc/detail/workTrackingCompletionHandler.hpp"

#include <cassert>
#include <optional>
#include <tuple>

AGRPC_NAMESPACE_BEGIN()

template <class... CompletionArgs>
class CancelSafe
{
  private:
    using CompletionSignature = void(detail::ErrorCode, CompletionArgs...);

  public:
    class CompletionToken
    {
      public:
        void operator()(CompletionArgs... completion_args)
        {
            if (auto&& ch = self.completion_handler.release())
            {
                std::move(ch).complete(detail::ErrorCode{}, std::move(completion_args)...);
            }
            else
            {
                self.result.emplace(std::move(completion_args)...);
            }
        }

      private:
        friend agrpc::CancelSafe<CompletionArgs...>;

        explicit CompletionToken(CancelSafe& self) noexcept : self(self) {}

        CancelSafe& self;
    };

    auto token() noexcept { return CompletionToken{*this}; }

    template <class CompletionToken>
    auto wait(CompletionToken token)
    {
        assert(!completion_handler && "Can only wait again when previous wait has been cancelled or completed");
        return asio::async_initiate<CompletionToken, CompletionSignature>(
            [&](auto&& ch)
            {
                auto executor = asio::get_associated_executor(ch);
                auto allocator = asio::get_associated_allocator(ch);
                if (result)
                {
                    auto local_result{std::move(*result)};
                    result.reset();
                    detail::post_with_allocator(
                        std::move(executor), std::move(allocator),
                        [local_result = std::move(local_result), ch = std::move(ch)]() mutable
                        {
                            std::apply(std::move(ch), std::tuple_cat(std::forward_as_tuple(detail::ErrorCode{}),
                                                                     std::move(local_result)));
                        });
                    return;
                }
                auto cancellation_slot = asio::get_associated_cancellation_slot(ch);
                this->emplace_completion_handler(std::move(ch));
                this->install_cancellation_handler(cancellation_slot);
            },
            token);
    }

  private:
    template <class CompletionHandler>
    void emplace_completion_handler(CompletionHandler&& ch)
    {
        using WorkTrackingCompletionHandler = detail::WorkTrackingCompletionHandler<std::decay_t<CompletionHandler>>;
        completion_handler.emplace(WorkTrackingCompletionHandler{std::forward<CompletionHandler>(ch)});
    }

    template <class CancellationSlot>
    void install_cancellation_handler(CancellationSlot& cancellation_slot)
    {
        if (cancellation_slot.is_connected())
        {
            cancellation_slot.assign(
                [this](asio::cancellation_type type)
                {
                    if (static_cast<bool>(type & asio::cancellation_type::all))
                    {
                        if (auto&& ch = completion_handler.release())
                        {
                            std::move(ch).post_complete(asio::error::operation_aborted, CompletionArgs{}...);
                        }
                    }
                });
        }
    }

    detail::AtomicTypeErasedCompletionHandler<void(detail::ErrorCode, CompletionArgs...)> completion_handler{};
    std::optional<std::tuple<CompletionArgs...>> result;
};

using GrpcCancelSafe = CancelSafe<bool>;

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_CANCELSAFE_HPP
