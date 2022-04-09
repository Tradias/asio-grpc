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

#include "agrpc/detail/cancelSafe.hpp"
#include "agrpc/detail/workTrackingCompletionHandler.hpp"

#include <atomic>
#include <cassert>
#include <optional>
#include <tuple>

AGRPC_NAMESPACE_BEGIN()

template <class... CompletionArgs>
struct CancelSafe
{
  private:
    using CompletionSignature = void(detail::ErrorCode, CompletionArgs...);

    struct CompletionToken
    {
        CancelSafe& self;

        void operator()(CompletionArgs... completion_args)
        {
            if (void* ch = self.completion_handler.exchange(nullptr))
            {
                self.complete(ch, detail::ErrorCode{}, std::move(completion_args)...);
            }
            else
            {
                self.result.emplace(std::move(completion_args)...);
            }
        }
    };

  public:
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
                this->allocate_and_assign_completion_handler(std::move(ch));
                this->install_cancellation_handler(cancellation_slot);
            },
            token);
    }

  private:
    template <class CompletionHandler>
    void allocate_and_assign_completion_handler(CompletionHandler&& ch)
    {
        using WorkTrackingCompletionHandler = detail::WorkTrackingCompletionHandler<std::decay_t<CompletionHandler>>;
        auto allocator{asio::get_associated_allocator(ch)};
        completion_handler =
            detail::allocate<WorkTrackingCompletionHandler>(allocator, std::forward<CompletionHandler>(ch)).release();
        complete = &detail::deallocate_and_invoke<WorkTrackingCompletionHandler, detail::ErrorCode, CompletionArgs...>;
        post_complete = &detail::post_and_complete<WorkTrackingCompletionHandler, detail::ErrorCode, CompletionArgs...>;
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
                        if (void* ch = completion_handler.exchange(nullptr))
                        {
                            post_complete(ch, asio::error::operation_aborted, CompletionArgs{}...);
                        }
                    }
                });
        }
    }

    using Complete = void (*)(void*, detail::ErrorCode, CompletionArgs...);

    std::atomic<void*> completion_handler{};
    Complete complete;
    Complete post_complete;
    std::optional<std::tuple<CompletionArgs...>> result;
};

using GrpcCancelSafe = CancelSafe<bool>;

AGRPC_NAMESPACE_END

#endif

#endif  // AGRPC_AGRPC_CANCELSAFE_HPP
