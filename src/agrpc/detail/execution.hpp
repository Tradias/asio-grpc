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

#ifndef AGRPC_DETAIL_EXECUTION_HPP
#define AGRPC_DETAIL_EXECUTION_HPP

#include <agrpc/detail/config.hpp>

#ifdef AGRPC_UNIFEX
#include <agrpc/detail/execution_unifex.hpp>
#elif defined(AGRPC_STDEXEC)
#include <agrpc/detail/execution_stdexec.hpp>
#elif defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
#include <agrpc/detail/execution_asio.hpp>
#endif

#endif  // AGRPC_DETAIL_EXECUTION_HPP
