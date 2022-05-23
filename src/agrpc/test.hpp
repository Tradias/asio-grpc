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

#ifndef AGRPC_AGRPC_TEST_HPP
#define AGRPC_AGRPC_TEST_HPP

#include <agrpc/detail/asioForward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpcContextImplementation.hpp>

AGRPC_NAMESPACE_BEGIN()

inline void process_tag_immediately(void* tag, bool ok, agrpc::GrpcContext& grpc_context)
{
    detail::process_tag(tag, detail::InvokeHandler::YES, ok, grpc_context);
}

inline void process_tag(void* tag, bool ok, agrpc::GrpcContext& grpc_context)
{
    asio::execution::execute(asio::require(grpc_context.get_executor(), asio::execution::blocking_t::never),
                             [&grpc_context, tag, ok]
                             {
                                 agrpc::process_tag_immediately(tag, ok, grpc_context);
                             });
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_TEST_HPP
