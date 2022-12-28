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

#include <agrpc/alarm.hpp>
#include <agrpc/detail/asio_forward.hpp>
#include <agrpc/detail/config.hpp>
#include <agrpc/detail/grpc_context_implementation.hpp>
#include <agrpc/grpc_context.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Test utility to manually process gRPC tags
 *
 * This function can be used to process gRPC tags in places where the tag does not go through the
 * `grpc::CompletionQueue`, e.g. in mocked stubs. It processes the tag in a manner equivalent to `asio::post`.
 *
 * Example using GMock:
 *
 * @snippet client.cpp mock-stub
 *
 * @since 1.7.0
 */
inline void process_grpc_tag(agrpc::GrpcContext& grpc_context, void* tag, bool ok)
{
    struct ProcessTag
    {
        agrpc::GrpcContext& grpc_context_;
        void* tag_;
        bool ok_;

        void operator()(bool, agrpc::Alarm&&)
        {
            detail::process_grpc_tag(tag_, ok_ ? detail::OperationResult::OK : detail::OperationResult::NOT_OK,
                                     grpc_context_);
        }
    };
    if (tag != nullptr)
    {
        agrpc::Alarm(grpc_context)
            .wait(detail::GrpcContextImplementation::TIME_ZERO, ProcessTag{grpc_context, tag, ok});
    }
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_TEST_HPP
