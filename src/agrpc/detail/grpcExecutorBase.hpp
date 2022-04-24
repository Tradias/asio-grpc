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

#ifndef AGRPC_DETAIL_GRPCEXECUTORBASE_HPP
#define AGRPC_DETAIL_GRPCEXECUTORBASE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpcContext.hpp>

#include <type_traits>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Allocator>
class GrpcExecutorBase
{
  protected:
    constexpr GrpcExecutorBase(agrpc::GrpcContext* grpc_context, Allocator allocator) noexcept
        : impl(grpc_context, std::move(allocator))
    {
    }

    constexpr decltype(auto) grpc_context() noexcept { return impl.first(); }

    constexpr decltype(auto) grpc_context() const noexcept { return impl.first(); }

    constexpr decltype(auto) allocator() noexcept { return impl.second(); }

    constexpr decltype(auto) allocator() const noexcept { return impl.second(); }

  private:
    detail::CompressedPair<agrpc::GrpcContext*, Allocator> impl;
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
        this->grpc_context()->work_started();
    }

    constexpr GrpcExecutorWorkTrackerBase(GrpcExecutorWorkTrackerBase&& other) noexcept
        : Base(std::exchange(other.grpc_context(), nullptr), std::move(other.allocator()))
    {
    }

    ~GrpcExecutorWorkTrackerBase() noexcept
    {
        if (this->grpc_context())
        {
            this->grpc_context()->work_finished();
        }
    }

    GrpcExecutorWorkTrackerBase& operator=(const GrpcExecutorWorkTrackerBase& other) noexcept
    {
        if (this != &other)
        {
            if (this->grpc_context())
            {
                this->grpc_context()->work_finished();
            }
            this->grpc_context() = other.grpc_context();
            if constexpr (std::is_assignable_v<Allocator&, const Allocator&>)
            {
                this->allocator() = other.allocator();
            }
            this->grpc_context()->work_started();
        }
        return *this;
    }

    GrpcExecutorWorkTrackerBase& operator=(GrpcExecutorWorkTrackerBase&& other) noexcept
    {
        if (this != &other)
        {
            if (this->grpc_context())
            {
                this->grpc_context()->work_finished();
            }
            this->grpc_context() = std::exchange(other.grpc_context(), nullptr);
            if constexpr (std::is_assignable_v<Allocator&, Allocator&&>)
            {
                this->allocator() = std::move(other.allocator());
            }
        }
        return *this;
    }

  protected:
    GrpcExecutorWorkTrackerBase(agrpc::GrpcContext* grpc_context, Allocator allocator) noexcept
        : Base(grpc_context, std::move(allocator))
    {
        this->grpc_context()->work_started();
    }
};
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPCEXECUTORBASE_HPP
