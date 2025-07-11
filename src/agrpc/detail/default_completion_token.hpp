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

#ifndef AGRPC_DETAIL_DEFAULT_COMPLETION_TOKEN_HPP
#define AGRPC_DETAIL_DEFAULT_COMPLETION_TOKEN_HPP

#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/use_sender.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#if defined(AGRPC_UNIFEX) || defined(AGRPC_STDEXEC)
template <class Executor>
using DefaultCompletionTokenT = detail::ConditionalT<std::is_same_v<void, asio::default_completion_token_t<Executor>>,
                                                     agrpc::UseSender, asio::default_completion_token_t<Executor>>;
#else
template <class Executor>
using DefaultCompletionTokenT = asio::default_completion_token_t<Executor>;
#endif
#else
template <class>
using DefaultCompletionTokenT = agrpc::UseSender;
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_DEFAULT_COMPLETION_TOKEN_HPP
