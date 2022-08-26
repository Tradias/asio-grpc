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
 * @file asio_grpc.hpp
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

#ifndef AGRPC_AGRPC_ASIO_GRPC_HPP
#define AGRPC_AGRPC_ASIO_GRPC_HPP

#include <agrpc/bind_allocator.hpp>
#include <agrpc/cancel_safe.hpp>
#include <agrpc/default_completion_token.hpp>
#include <agrpc/get_completion_queue.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/grpc_initiate.hpp>
#include <agrpc/grpc_stream.hpp>
#include <agrpc/high_level_client.hpp>
#include <agrpc/repeatedly_request.hpp>
#include <agrpc/repeatedly_request_context.hpp>
#include <agrpc/rpc.hpp>
#include <agrpc/rpc_type.hpp>
#include <agrpc/run.hpp>
#include <agrpc/test.hpp>
#include <agrpc/use_awaitable.hpp>
#include <agrpc/use_sender.hpp>
#include <agrpc/wait.hpp>

#endif  // AGRPC_AGRPC_ASIO_GRPC_HPP
