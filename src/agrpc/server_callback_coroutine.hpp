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

#ifndef AGRPC_AGRPC_SERVER_CALLBACK_COROUTINE_HPP
#define AGRPC_AGRPC_SERVER_CALLBACK_COROUTINE_HPP

#include <agrpc/detail/server_callback_coroutine.hpp>
#include <grpcpp/server_context.h>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

inline constexpr detail::GetReactorArg get_reactor{};

inline constexpr detail::InitiateSendInitialMetadataArg initiate_send_initial_metadata{};

inline constexpr detail::WaitForSendInitialMetadataArg wait_for_send_initial_metadata{};

inline constexpr detail::WaitForFinishArg wait_for_finish{};

[[nodiscard]] inline detail::InitiateFinishArg initiate_finish(grpc::Status status) noexcept
{
    return {std::move(status)};
}

template <class Request>
[[nodiscard]] inline detail::InitiateReadArg<Request> initiate_read(Request& request) noexcept
{
    return {request};
}

inline constexpr detail::WaitForReadArg wait_for_read{};

AGRPC_NAMESPACE_END

template <class Service, class Request, class Response>
struct std::coroutine_traits<grpc::ServerUnaryReactor*, Service, grpc::CallbackServerContext*, const Request*,
                             Response*>
{
    using promise_type = agrpc::detail::ServerReactorPromiseType<agrpc::ServerUnaryReactor>;
};

template <class Service, class Request, class Response>
struct std::coroutine_traits<grpc::ServerReadReactor<Request>*, Service, grpc::CallbackServerContext*, Response*>
{
    using promise_type = agrpc::detail::ServerReactorPromiseType<agrpc::ServerReadReactor<Request>>;
};

#endif  // AGRPC_AGRPC_SERVER_CALLBACK_COROUTINE_HPP
