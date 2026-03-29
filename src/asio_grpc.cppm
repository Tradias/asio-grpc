// Copyright 2026 Dennis Hezel
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

module;

// ── Global module fragment ────────────────────────────────────────────────────
// Everything #include'd here is attached to the *global* module, not to the
// named module below. Macro definitions from these headers are visible within
// this translation unit but are NOT propagated to importers — which is exactly
// what we want: the backend macros drive compilation here and stay private.
//
// Template specialisations defined in this section (e.g.
//   std::uses_allocator<agrpc::GrpcContext, A>  in grpc_context.hpp
//   std::uses_allocator<agrpc::BasicGrpcExecutor<…>, A>  in grpc_executor.hpp
//   agrpc::asio::traits::*  specialisations in grpc_executor.hpp
// ) are reachable to importers through the exported types they are
// specialised on, per [temp.spec.partial.general]/6 and [module.reach].

#include <agrpc/asio_grpc.hpp>
#include <agrpc/client_callback.hpp>
#include <agrpc/reactor_ptr.hpp>
#include <agrpc/server_callback.hpp>

// ── Named module interface ────────────────────────────────────────────────────
export module asio_grpc;

export namespace agrpc
{
using agrpc::Alarm;
using agrpc::allocate_reactor;
using agrpc::BasicAlarm;
using agrpc::BasicClientBidiReactor;
using agrpc::BasicClientReadReactor;
using agrpc::BasicClientUnaryReactor;
using agrpc::BasicClientWriteReactor;
using agrpc::BasicGrpcExecutor;
using agrpc::BasicServerBidiReactor;
using agrpc::BasicServerReadReactor;
using agrpc::BasicServerUnaryReactor;
using agrpc::BasicServerWriteReactor;
using agrpc::ClientBidiReactor;
using agrpc::ClientReadReactor;
using agrpc::ClientRPC;
using agrpc::ClientRPCType;
using agrpc::ClientUnaryReactor;
using agrpc::ClientWriteReactor;
using agrpc::DefaultServerRPCTraits;
using agrpc::GenericServerRPC;
using agrpc::GenericStreamingClientRPC;
using agrpc::GenericUnaryClientRPC;
using agrpc::GrpcContext;
using agrpc::GrpcExecutor;
using agrpc::make_reactor;
using agrpc::notify_on_state_change;
using agrpc::process_grpc_tag;
using agrpc::ReactorPtr;
using agrpc::read;
using agrpc::register_sender_rpc_handler;
using agrpc::ServerBidiReactor;
using agrpc::ServerReadReactor;
using agrpc::ServerRPC;
using agrpc::ServerRPCPtr;
using agrpc::ServerRPCType;
using agrpc::ServerUnaryReactor;
using agrpc::ServerWriteReactor;
using agrpc::unary_call;
using agrpc::use_sender;
using agrpc::UseSender;
using agrpc::Waiter;

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
using agrpc::DefaultRunTraits;
using agrpc::register_callback_rpc_handler;
using agrpc::register_yield_rpc_handler;
using agrpc::run;
using agrpc::run_completion_queue;
#ifdef AGRPC_ASIO_HAS_CO_AWAIT
using agrpc::register_awaitable_rpc_handler;
using agrpc::register_coroutine_rpc_handler;
#endif
#endif
}