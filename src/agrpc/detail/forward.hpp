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

#ifndef AGRPC_DETAIL_FORWARD_HPP
#define AGRPC_DETAIL_FORWARD_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_executor_options.hpp>

#include <memory>

namespace grpc
{
class CompletionQueue;
class ClientContext;
}

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

namespace detail
{
template <class Item>
class IntrusiveQueue;

template <class Item>
class AtomicIntrusiveQueue;

struct BasicSenderAccess;

template <class Sender, class Receiver, class... CompletionArgs>
class ConditionalSenderOperationState;

class RepeatedlyRequestFn;

struct RepeatedlyRequestContextAccess;

class GenericRPCContext;

class NotfiyWhenDoneSenderImplementation;

struct HealthCheckServiceData;

class HealthCheckWatcher;

class HealthCheckChecker;

template <class Derived, class Response>
class ServerWriteReactor;

template <class Deadline, class Executor>
struct MoveAlarmSenderImplementation;

template <class Responder, class Executor>
class RPCClientClientStreamingBase;

template <class Responder, class Executor>
class RPCClientServerStreamingBase;

template <class Responder, class Executor>
class RPCBidirectionalStreamingBase;

template <class Allocator, std::uint32_t Options>
grpc::CompletionQueue* get_completion_queue(const agrpc::BasicGrpcExecutor<Allocator, Options>&) noexcept;

grpc::CompletionQueue* get_completion_queue(agrpc::GrpcContext&) noexcept;

template <class Executor>
grpc::CompletionQueue* get_completion_queue(const agrpc::BasicGrpcStream<Executor>&) noexcept;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_FORWARD_HPP
