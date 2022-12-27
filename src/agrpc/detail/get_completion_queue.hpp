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

#ifndef AGRPC_DETAIL_GET_COMPLETION_QUEUE_HPP
#define AGRPC_DETAIL_GET_COMPLETION_QUEUE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/forward.hpp>
#include <grpcpp/completion_queue.h>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Allocator, std::uint32_t Options>
grpc::CompletionQueue* get_completion_queue(const agrpc::BasicGrpcExecutor<Allocator, Options>&) noexcept;

grpc::CompletionQueue* get_completion_queue(agrpc::GrpcContext&) noexcept;

template <class Executor>
grpc::CompletionQueue* get_completion_queue(const agrpc::BasicGrpcStream<Executor>&) noexcept;
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GET_COMPLETION_QUEUE_HPP
