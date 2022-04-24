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

/**
 * @file asioGrpc.hpp
 * @brief Convenience header
 */

/**
 * @namespace agrpc
 * @brief Public namespace
 */

/**
 * @namespace agrpc::pmr
 * @brief Public namespace
 */

#ifndef AGRPC_AGRPC_ASIOGRPC_HPP
#define AGRPC_AGRPC_ASIOGRPC_HPP

#include <agrpc/bindAllocator.hpp>
#include <agrpc/cancelSafe.hpp>
#include <agrpc/defaultCompletionToken.hpp>
#include <agrpc/getCompletionQueue.hpp>
#include <agrpc/grpcContext.hpp>
#include <agrpc/grpcExecutor.hpp>
#include <agrpc/grpcInitiate.hpp>
#include <agrpc/pollContext.hpp>
#include <agrpc/repeatedlyRequest.hpp>
#include <agrpc/repeatedlyRequestContext.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/useAwaitable.hpp>
#include <agrpc/useSender.hpp>
#include <agrpc/wait.hpp>

#endif  // AGRPC_AGRPC_ASIOGRPC_HPP
