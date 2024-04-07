// Copyright 2024 Dennis Hezel
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

#if !defined(AGRPC_ASIO_HAS_CANCELLATION_SLOT) && !defined(AGRPC_ASIO_HAS_NEW_SPAWN) && \
    !defined(AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR)

#ifdef AGRPC_STANDALONE_ASIO  // standalone Asio
#include <asio/version.hpp>

#if (ASIO_VERSION >= 101900)
#define AGRPC_ASIO_HAS_CANCELLATION_SLOT
#endif

#if (ASIO_VERSION >= 102400)
#define AGRPC_ASIO_HAS_NEW_SPAWN
#endif

#if (ASIO_VERSION >= 102700)
#define AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR
#endif
#elif defined(AGRPC_BOOST_ASIO)  // Boost.Asio
#include <boost/version.hpp>

#if (BOOST_VERSION >= 107700)
#define AGRPC_ASIO_HAS_CANCELLATION_SLOT
#endif

#if (BOOST_VERSION >= 108000)
#define AGRPC_ASIO_HAS_NEW_SPAWN
#endif

#if (BOOST_VERSION >= 108200)
#define AGRPC_ASIO_HAS_IMMEDIATE_EXECUTOR
#endif
#endif

#endif
