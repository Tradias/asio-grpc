// Copyright 2024 Dennis Hezel
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
#include <agrpc/detail/submit.hpp>
#include <agrpc/detail/test.hpp>
#include <agrpc/grpc_context.hpp>

#include <agrpc/detail/config.hpp>

AGRPC_NAMESPACE_BEGIN()

/**
 * @brief Test utility to manually process gRPC tags
 *
 * This function can be used to process gRPC tags in places where the tag does not go through the
 * `grpc::CompletionQueue`, for example in mocked stubs. It processes the tag in a manner equivalent to `asio::post`
 * while being compatible with `GrpcContext::run_completion_queue()`/`GrpcContext::poll_completion_queue()`.
 *
 * Example using Google Mock:
 *
 * @snippet client.cpp mock-stub
 *
 * @since 1.7.0
 */
inline void process_grpc_tag(agrpc::GrpcContext& grpc_context, void* tag, bool ok)
{
    if (tag != nullptr)
    {
        detail::ProcessTag process_tag{grpc_context, tag, ok};
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
        agrpc::Alarm(grpc_context).wait(detail::GrpcContextImplementation::TIME_ZERO, process_tag);
#else
        detail::submit_to_function(
            agrpc::Alarm(grpc_context).wait(detail::GrpcContextImplementation::TIME_ZERO, agrpc::use_sender),
            process_tag);
#endif
    }
}

AGRPC_NAMESPACE_END

#include <agrpc/detail/epilogue.hpp>

#endif  // AGRPC_AGRPC_TEST_HPP
