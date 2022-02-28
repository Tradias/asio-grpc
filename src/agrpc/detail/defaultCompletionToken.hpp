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

#ifndef AGRPC_DETAIL_DEFAULTCOMPLETIONTOKEN_HPP
#define AGRPC_DETAIL_DEFAULTCOMPLETIONTOKEN_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/config.hpp"

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
struct DefaultCompletionTokenNotAvailable
{
    DefaultCompletionTokenNotAvailable() = delete;
};

#ifdef AGRPC_ASIO_HAS_CO_AWAIT
using DefaultCompletionToken = asio::use_awaitable_t<>;
#else
using DefaultCompletionToken = detail::DefaultCompletionTokenNotAvailable;
#endif
}

AGRPC_NAMESPACE_END

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
AGRPC_ASIO_NAMESPACE_BEGIN()

template <class Signature>
class async_result<::agrpc::detail::DefaultCompletionTokenNotAvailable, Signature>
{
};

AGRPC_ASIO_NAMESPACE_END
#endif

#endif  // AGRPC_DETAIL_DEFAULTCOMPLETIONTOKEN_HPP
