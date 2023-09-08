// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_FORWARD_HPP
#define AGRPC_DETAIL_FORWARD_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_executor_options.hpp>
#include <agrpc/detail/namespace_cpp20.hpp>

#include <memory>

AGRPC_NAMESPACE_BEGIN()

template <class Allocator = std::allocator<void>, std::uint32_t Options = detail::GrpcExecutorOptions::DEFAULT>
class BasicGrpcExecutor;

class GrpcContext;

class HealthCheckService;

template <class Executor>
class BasicGrpcStream;

template <class Executor>
class BasicAlarm;

struct UseSender;

/**
 * @brief (experimental) Primary ClientRPC template
 *
 * This is the main entrypoint into the recommended API for writing asynchronous gRPC clients.
 *
 * @see
 * @c agrpc::ClientRPC<PrepareAsyncUnary,Executor> <br>
 * @c agrpc::ClientRPC<agrpc::ClientRPCType::GENERIC_UNARY,Executor> <br>
 * @c agrpc::ClientRPC<PrepareAsyncClientStreaming,Executor> <br>
 * @c agrpc::ClientRPC<PrepareAsyncServerStreaming,Executor> <br>
 * @c agrpc::ClientRPC<PrepareAsyncBidiStreaming,Executor> <br>
 * @c agrpc::ClientRPC<agrpc::ClientRPCType::GENERIC_STREAMING,Executor> <br>
 *
 * @since 2.6.0
 */
template <auto PrepareAsync, class Executor = agrpc::BasicGrpcExecutor<>>
class ClientRPC;

struct DefaultServerRPCTraits;

/**
 * @brief (experimental) Primary ServerRPC template
 *
 * This is the main entrypoint into the recommended API for writing asynchronous gRPC clients.
 *
 * @see
 * @c agrpc::ServerRPC<RequestAsyncUnary,Executor> <br>
 * @c agrpc::ServerRPC<RequestAsyncClientStreaming,Executor> <br>
 * @c agrpc::ServerRPC<RequestAsyncServerStreaming,Executor> <br>
 * @c agrpc::ServerRPC<RequestAsyncBidiStreaming,Executor> <br>
 * @c agrpc::ServerRPC<agrpc::ServerRPCType::GENERIC_STREAMING,Executor> <br>
 *
 * @since 2.7.0
 */
template <auto RequestRPC, class Traits = agrpc::DefaultServerRPCTraits, class Executor = agrpc::BasicGrpcExecutor<>>
class ServerRPC;

namespace detail
{
template <class Item>
class IntrusiveQueue;

template <class Item>
class AtomicIntrusiveQueue;

struct BasicSenderAccess;

struct RepeatedlyRequestContextAccess;

class GenericRPCContext;

struct NotifyWhenDoneSenderImplementation;

struct HealthCheckServiceData;

class HealthCheckWatcher;

class HealthCheckChecker;

template <class Derived, class Response>
class ServerWriteReactor;

template <class Executor>
struct MoveAlarmSenderImplementation;

template <auto PrepareAsync, class Executor>
class ClientRPCServerStreamingBase;

template <class Responder, class Executor>
class ClientRPCBidiStreamingBase;

template <class Responder, class Traits, class Executor>
class ServerRPCBidiStreamingBase;

struct ClientRPCContextBaseAccess;

struct ServerRPCContextBaseAccess;

struct RPCExecutorBaseAccess;

template <bool IsNotifyWhenDone, class Responder, class Executor>
class ServerRPCNotifyWhenDoneMixin;

template <class Signature>
class RunningManualResetEvent;

AGRPC_NAMESPACE_CPP20_BEGIN()

class RepeatedlyRequestFn;

AGRPC_NAMESPACE_CPP20_END
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_FORWARD_HPP
