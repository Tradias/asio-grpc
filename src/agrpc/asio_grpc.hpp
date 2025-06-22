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

/**
 * @file asio_grpc.hpp
 * @brief Convenience header
 */

/**
 * @namespace agrpc
 * @brief Public namespace
 */

#ifndef AGRPC_AGRPC_ASIO_GRPC_HPP
#define AGRPC_AGRPC_ASIO_GRPC_HPP

#include <agrpc/alarm.hpp>
#include <agrpc/client_rpc.hpp>
#include <agrpc/default_server_rpc_traits.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <agrpc/notify_on_state_change.hpp>
#include <agrpc/read.hpp>
#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <agrpc/register_callback_rpc_handler.hpp>
#include <agrpc/register_coroutine_rpc_handler.hpp>
#include <agrpc/register_sender_rpc_handler.hpp>
#include <agrpc/rpc_type.hpp>
#include <agrpc/run.hpp>
#include <agrpc/server_rpc.hpp>
#include <agrpc/test.hpp>
#include <agrpc/use_sender.hpp>
#include <agrpc/waiter.hpp>

#endif  // AGRPC_AGRPC_ASIO_GRPC_HPP
