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

#ifndef AGRPC_AGRPC_DEFAULTCOMPLETIONTOKEN_HPP
#define AGRPC_AGRPC_DEFAULTCOMPLETIONTOKEN_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/defaultCompletionToken.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Default completion token for all asynchronous functions
 *
 * Only available for Boost.Asio and standalone Asio: `asio::use_awaitable`
 */
using DefaultCompletionToken = detail::DefaultCompletionToken;

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_DEFAULTCOMPLETIONTOKEN_HPP
