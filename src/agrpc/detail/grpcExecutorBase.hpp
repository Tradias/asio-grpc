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

#ifndef AGRPC_DETAIL_GRPCEXECUTORBASE_HPP
#define AGRPC_DETAIL_GRPCEXECUTORBASE_HPP

#include "agrpc/detail/utility.hpp"
#include "agrpc/grpcContext.hpp"

#include <utility>

namespace agrpc::detail
{
template <class Allocator>
class GrpcExecutorBase
{
  public:
    using allocator_type = Allocator;

    void on_work_started() const noexcept { this->grpc_context()->work_started(); }

    void on_work_finished() const noexcept { this->grpc_context()->work_finished(); }

  protected:
    constexpr GrpcExecutorBase(agrpc::GrpcContext* grpc_context, allocator_type allocator) noexcept
        : impl(grpc_context, std::move(allocator))
    {
    }

    constexpr decltype(auto) grpc_context() noexcept { return impl.first(); }

    constexpr decltype(auto) grpc_context() const noexcept { return impl.first(); }

    constexpr decltype(auto) allocator() noexcept { return impl.second(); }

    constexpr decltype(auto) allocator() const noexcept { return impl.second(); }

  private:
    detail::CompressedPair<agrpc::GrpcContext*, allocator_type> impl;
};

template <class Allocator>
class GrpcExecutorWorkTrackerBase : public detail::GrpcExecutorBase<Allocator>
{
  private:
    using Base = detail::GrpcExecutorBase<Allocator>;

  public:
    GrpcExecutorWorkTrackerBase(const GrpcExecutorWorkTrackerBase& other) noexcept
        : Base(other.grpc_context(), other.allocator())
    {
        if (this->grpc_context())
        {
            this->on_work_started();
        }
    }

    constexpr GrpcExecutorWorkTrackerBase(GrpcExecutorWorkTrackerBase&& other) noexcept
        : Base(other.grpc_context(), std::move(other.allocator()))
    {
        other.grpc_context() = nullptr;
    }

    ~GrpcExecutorWorkTrackerBase() noexcept
    {
        if (this->grpc_context())
        {
            this->on_work_finished();
        }
    }

    GrpcExecutorWorkTrackerBase& operator=(const GrpcExecutorWorkTrackerBase& other) noexcept
    {
        if (this != std::addressof(other))
        {
            auto* old_grpc_context = this->grpc_context();
            this->grpc_context() = other.grpc_context();
            this->allocator() = other.allocator();
            if (this->grpc_context())
            {
                this->grpc_context()->work_started();
            }
            if (old_grpc_context)
            {
                old_grpc_context->work_finished();
            }
        }
        return *this;
    }

    GrpcExecutorWorkTrackerBase& operator=(GrpcExecutorWorkTrackerBase&& other) noexcept
    {
        if (this != std::addressof(other))
        {
            auto* old_grpc_context = this->grpc_context();
            this->grpc_context() = other.grpc_context();
            this->allocator() = std::move(other.allocator());
            other.grpc_context() = nullptr;
            if (old_grpc_context)
            {
                old_grpc_context->work_finished();
            }
        }
        return *this;
    }

  protected:
    GrpcExecutorWorkTrackerBase(agrpc::GrpcContext* grpc_context, Allocator allocator) noexcept
        : Base(grpc_context, std::move(allocator))
    {
        if (this->grpc_context())
        {
            this->on_work_started();
        }
    }
};
}  // namespace agrpc::detail

#endif  // AGRPC_DETAIL_GRPCEXECUTORBASE_HPP
