// Copyright 2026 Dennis Hezel
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

#ifndef AGRPC_DETAIL_SUBMIT_HPP
#define AGRPC_DETAIL_SUBMIT_HPP

#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Sender, class Function>
struct SubmitToFunctionReceiver
{
    struct Wrap
    {
    #ifdef AGRPC_STDEXEC
        using receiver_concept = exec::receiver_t;
#endif
        using is_receiver = void;

        void set_done() noexcept { complete(); }

        void set_stopped() noexcept { complete(); }

        template <class... Args>
        void set_value(Args&&... args) noexcept
        {
            complete(static_cast<Args&&>(args)...);
        }

        template <class E>
        void set_error(E&& e) noexcept
        {
            complete(static_cast<E&&>(e));
        }

        template <class... Args>
        void complete(Args&&... args)
        {
            detail::ScopeGuard guard{[&]
                                     {
                                         delete p_;
                                     }};
            p_->function_(static_cast<Args&&>(args)...);
        }

        SubmitToFunctionReceiver* p_;
    };

    template <class S, class F>
    SubmitToFunctionReceiver(S&& sender, F&& function)
        : function_(static_cast<F&&>(function)), state_(exec::connect(static_cast<S&&>(sender), Wrap{this}))
    {
    }

    Function function_;
    exec::connect_result_t<Sender, Wrap> state_;
};

template <class Sender, class Function>
void submit_to_function(Sender&& sender, Function&& function)
{
    exec::start((new SubmitToFunctionReceiver<detail::RemoveCrefT<Sender>, detail::RemoveCrefT<Function>>(
                     static_cast<Sender&&>(sender), static_cast<Function&&>(function)))
                    ->state_);
}
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SUBMIT_HPP
