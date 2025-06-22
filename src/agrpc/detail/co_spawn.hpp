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

#ifndef AGRPC_DETAIL_CO_SPAWN_HPP
#define AGRPC_DETAIL_CO_SPAWN_HPP

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#endif

#endif  // AGRPC_DETAIL_CO_SPAWN_HPP
