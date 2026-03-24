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

// C++20 module interface unit for asio-grpc.
//
// The backend is selected at CMake configure time via compile definitions
// (AGRPC_BOOST_ASIO / AGRPC_STANDALONE_ASIO / AGRPC_UNIFEX / AGRPC_STDEXEC),
// exactly as for the existing INTERFACE targets. Each CMake module target
// (asio-grpc-module, asio-grpc-standalone-asio-module, …) links to the
// corresponding legacy INTERFACE target and therefore inherits the right
// definition automatically.
//
// Usage (CMake):
//   target_link_libraries(my_target PRIVATE asio-grpc::asio-grpc-module)
//
// Usage (C++):
//   import asio_grpc;

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

// ── Named module interface ────────────────────────────────────────────────────
export module asio_grpc;

// Re-export every public name from the agrpc namespace.
//
// All backend inline namespaces (b / s / u / e and their combinations) are
// *inline* in agrpc, so every name is already directly reachable as
// agrpc::Name. The export using-declarations below make those names part of
// this module's exported interface and ensure that template specialisations
// on these types (e.g. std::uses_allocator) are reachable to importers.

// -- GrpcContext ---------------------------------------------------------------
export using agrpc::GrpcContext;

// -- GrpcExecutor --------------------------------------------------------------
export using agrpc::BasicGrpcExecutor;
export using agrpc::GrpcExecutor;

// -- Alarm ---------------------------------------------------------------------
export using agrpc::BasicAlarm;
export using agrpc::Alarm;

// -- RPC types (enums) ---------------------------------------------------------
export using agrpc::ClientRPCType;
export using agrpc::ServerRPCType;

// -- ClientRPC / ServerRPC -----------------------------------------------------
export using agrpc::ClientRPC;
export using agrpc::ServerRPC;

// -- Traits --------------------------------------------------------------------
export using agrpc::DefaultServerRPCTraits;

// -- Completion token / sender -------------------------------------------------
export using agrpc::UseSender;
export using agrpc::use_sender;

// -- Waiter --------------------------------------------------------------------
export using agrpc::Waiter;

// -- CPO-style function objects ------------------------------------------------
export using agrpc::notify_on_state_change;
export using agrpc::read;

// -- run helpers ---------------------------------------------------------------
export using agrpc::DefaultRunTraits;
export using agrpc::run;
export using agrpc::run_completion_queue;

// -- RPC handler registration --------------------------------------------------
export using agrpc::register_awaitable_rpc_handler;
export using agrpc::register_callback_rpc_handler;
export using agrpc::register_coroutine_rpc_handler;
export using agrpc::register_sender_rpc_handler;

// -- Test utilities ------------------------------------------------------------
export using agrpc::process_grpc_tag;
