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

#ifndef AGRPC_ASIO_HAS_CO_AWAIT

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/awaitable.hpp>

#ifdef ASIO_HAS_CO_AWAIT
#define AGRPC_ASIO_HAS_CO_AWAIT
#endif
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/awaitable.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#define AGRPC_ASIO_HAS_CO_AWAIT
#endif
#endif

#endif
