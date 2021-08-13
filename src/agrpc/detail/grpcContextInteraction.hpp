// Copyright 2021 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
#define AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/attributes.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/grpcExecutorOperation.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/grpcContext.hpp"

namespace agrpc::detail
{
template <class Allocator>
[[nodiscard]] constexpr const auto& get_local_allocator(agrpc::GrpcContext&, const Allocator& allocator) noexcept
{
    return allocator;
}

template <class T>
[[nodiscard]] auto get_local_allocator(agrpc::GrpcContext& grpc_context, const std::allocator<T>&) noexcept
{
    return grpc_context.get_allocator();
}

template <class Function, class Allocator>
auto allocate_work(Function&& function, const Allocator& allocator)
{
    using DecayedFunction = std::decay_t<Function>;
    using Operation = detail::GrpcExecutorOperation<DecayedFunction, Allocator>;
    auto ptr = detail::allocate_unique<Operation>(allocator, std::forward<Function>(function), allocator);
    return ptr;
}

template <bool IsBlockingNever, class Function, class OnLocalWork, class OnRemoteWork, class WorkAllocator>
void create_work(agrpc::GrpcContext& grpc_context, Function&& function, OnLocalWork&& on_local_work,
                 OnRemoteWork&& on_remote_work, const WorkAllocator& work_allocator)
{
    if (grpc_context.is_stopped()) AGRPC_UNLIKELY
        {
            return;
        }
    if (detail::GrpcContextImplementation::running_in_this_thread(grpc_context))
    {
        if constexpr (IsBlockingNever)
        {
            auto&& local_allocator = detail::get_local_allocator(grpc_context, work_allocator);
            auto ptr = detail::allocate_work(std::forward<Function>(function), local_allocator);
            on_local_work(ptr.get());
            ptr.release();
        }
        else
        {
            using DecayedFunction = std::decay_t<Function>;
            DecayedFunction temp{std::forward<Function>(function)};
            temp();
        }
    }
    else
    {
        auto ptr = detail::allocate_work(std::forward<Function>(function), work_allocator);
        on_remote_work(ptr.get());
        ptr.release();
    }
}

template <bool IsBlockingNever, class Function, class OnWork, class WorkAllocator>
void create_work(agrpc::GrpcContext& grpc_context, Function&& function, OnWork&& on_work,
                 const WorkAllocator& work_allocator)
{
    detail::create_work<IsBlockingNever>(grpc_context, std::forward<Function>(function), on_work, on_work,
                                         work_allocator);
}

struct AddToLocalWork
{
    agrpc::GrpcContext& grpc_context;

    void operator()(detail::GrpcContextOperation* work)
    {
        detail::GrpcContextImplementation::add_local_work(grpc_context, work);
    }
};

struct AddToRemoteWork
{
    agrpc::GrpcContext& grpc_context;

    void operator()(detail::GrpcContextOperation* work)
    {
        detail::GrpcContextImplementation::add_remote_work(grpc_context, work);
    }
};

template <bool IsBlockingNever, class Function, class WorkAllocator>
void create_work(agrpc::GrpcContext& grpc_context, Function&& function, const WorkAllocator& work_allocator)
{
    detail::create_work<IsBlockingNever>(grpc_context, std::forward<Function>(function), AddToLocalWork{grpc_context},
                                         AddToRemoteWork{grpc_context}, work_allocator);
}
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCCONTEXTINTERACTION_HPP
