// Copyright 2025 Dennis Hezel
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

#ifndef AGRPC_DETAIL_SENDER_CREATE_HPP
#define AGRPC_DETAIL_SENDER_CREATE_HPP

#include <agrpc/detail/execution.hpp>
#include <agrpc/detail/sender_of.hpp>
#include <agrpc/detail/utility.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
namespace create_ns
{
template <class Receiver, class Fn, class... ValueTypes>
struct OperationState
{
    OperationState(Receiver rec, Fn fn) : rec_(static_cast<Receiver&&>(rec)), fn_(static_cast<Fn&&>(fn)) {}

    template <class... Ts>
    void set_value(Ts&&... ts) noexcept
    {
        AGRPC_TRY
        {
            // Satisfy the value completion contract by converting to the
            // Sender's value_types. For example, if set_value is called with
            // an lvalue reference but the create Sender sends non-reference
            // values.
            exec::set_value(std::move(rec_), static_cast<ValueTypes>(static_cast<Ts&&>(ts))...);
        }
        AGRPC_CATCH(...) { exec::set_error(std::move(rec_), std::current_exception()); }
    }

    template <class Error>
    void set_error(Error&& error) noexcept
    {
        exec::set_error(static_cast<Receiver&&>(rec_), static_cast<Error&&>(error));
    }

    void set_done() noexcept { exec::set_done(static_cast<Receiver&&>(rec_)); }

    void start() noexcept
    {
        try
        {
            fn_(*this);
        }
        catch (...)
        {
            exec::set_error(static_cast<Receiver&&>(rec_), std::current_exception());
        }
    }

  private:
#ifdef AGRPC_STDEXEC
    friend void tag_invoke(stdexec::start_t, OperationState& s) noexcept { s.start(); }
#endif

    Receiver rec_;
    Fn fn_;
};

#ifdef AGRPC_STDEXEC
struct InlineSchedulerEnv
{
    template <class Tag>
    friend constexpr exec::inline_scheduler tag_invoke(stdexec::get_completion_scheduler_t<Tag>,
                                                       const InlineSchedulerEnv&) noexcept
    {
        return {};
    }
};
#endif

template <class Fn, class... ValueTypes>
class Sender : public detail::SenderOf<void(ValueTypes...)>
{
  public:
    explicit Sender(Fn fn) : fn_(static_cast<Fn&&>(fn)) {}

    template <class Receiver>
    [[nodiscard]] OperationState<detail::RemoveCrefT<Receiver>, Fn, ValueTypes...> connect(
        Receiver&& receiver) && noexcept(std::is_nothrow_move_constructible_v<Fn>)
    {
        return {static_cast<Receiver&&>(receiver), static_cast<Fn&&>(fn_)};
    }

#ifdef AGRPC_STDEXEC
    template <class Receiver>
    friend auto tag_invoke(stdexec::connect_t, Sender&& s,
                           Receiver&& r) noexcept(noexcept(s.connect(static_cast<Receiver&&>(r))))
    {
        return static_cast<Sender&&>(s).connect(static_cast<Receiver&&>(r));
    }

    friend InlineSchedulerEnv tag_invoke(stdexec::get_env_t, const Sender&) noexcept { return {}; }
#endif

  private:
    Fn fn_;
};

template <class... ValueTypes>
struct Fn
{
    template <class Fn>
    Sender<Fn, ValueTypes...> operator()(Fn fn) const
        noexcept(std::is_nothrow_constructible_v<Sender<Fn, ValueTypes...>, Fn>)
    {
        return Sender<Fn, ValueTypes...>{static_cast<Fn&&>(fn)};
    }
};
}

template <class... ValueTypes>
inline constexpr create_ns::Fn<ValueTypes...> sender_create{};
}  // namespace detail

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_SENDER_CREATE_HPP
