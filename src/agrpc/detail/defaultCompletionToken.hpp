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

#ifdef AGRPC_STANDALONE_ASIO
namespace asio
{
template <class Signature>
class async_result<::agrpc::detail::DefaultCompletionTokenNotAvailable, Signature>
{
};
}  // namespace asio
#elif defined(AGRPC_BOOST_ASIO)
namespace boost::asio
{
template <class Signature>
class async_result<::agrpc::detail::DefaultCompletionTokenNotAvailable, Signature>
{
};
}  // namespace boost::asio
#endif

#endif  // AGRPC_DETAIL_DEFAULTCOMPLETIONTOKEN_HPP
