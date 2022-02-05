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

#ifndef AGRPC_DETAIL_NOOPRECEIVERWITHALLOCATOR_HPP
#define AGRPC_DETAIL_NOOPRECEIVERWITHALLOCATOR_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/utility.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Allocator>
class NoOpReceiverWithAllocator : detail::EmptyBaseOptimization<Allocator>
{
  public:
    using allocator_type = Allocator;

    constexpr explicit NoOpReceiverWithAllocator(Allocator allocator) noexcept
        : detail::EmptyBaseOptimization<Allocator>(allocator)
    {
    }

    static constexpr void set_done() noexcept {}

    template <class... Args>
    static constexpr void set_value(Args&&...) noexcept
    {
    }

    static void set_error(std::exception_ptr) noexcept {}

    constexpr auto& get_allocator() const noexcept { return this->get(); }

#ifdef AGRPC_UNIFEX
    friend constexpr auto tag_invoke(unifex::tag_t<unifex::get_allocator>,
                                     const NoOpReceiverWithAllocator& receiver) noexcept
    {
        return receiver.get();
    }
#endif
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_NOOPRECEIVERWITHALLOCATOR_HPP
