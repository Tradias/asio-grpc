// Copyright 2023 Dennis Hezel
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

#ifndef AGRPC_DETAIL_GRPC_EXECUTOR_BASE_HPP
#define AGRPC_DETAIL_GRPC_EXECUTOR_BASE_HPP

#include <agrpc/detail/config.hpp>
#include <agrpc/detail/utility.hpp>
#include <agrpc/grpc_context.hpp>

#include <type_traits>
#include <utility>

AGRPC_NAMESPACE_BEGIN()

namespace detail
{
template <class Allocator>
class GrpcExecutorBase
{
  protected:
    GrpcExecutorBase() = default;

    GrpcExecutorBase(agrpc::GrpcContext* grpc_context, const Allocator& allocator) noexcept
        : impl_(grpc_context, allocator)
    {
    }

    [[nodiscard]] agrpc::GrpcContext*& grpc_context() noexcept { return impl_.first(); }

    [[nodiscard]] agrpc::GrpcContext* grpc_context() const noexcept { return impl_.first(); }

    [[nodiscard]] Allocator& allocator() noexcept { return impl_.second(); }

    [[nodiscard]] const Allocator& allocator() const noexcept { return impl_.second(); }

  private:
    detail::CompressedPair<agrpc::GrpcContext*, Allocator> impl_;
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

    GrpcExecutorWorkTrackerBase(GrpcExecutorWorkTrackerBase&& other) noexcept
        : Base(std::exchange(other.grpc_context(), nullptr), other.allocator())
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
                this->allocator() = static_cast<Allocator&&>(other.allocator());
            }
        }
        return *this;
    }

  protected:
    GrpcExecutorWorkTrackerBase() = default;

    GrpcExecutorWorkTrackerBase(agrpc::GrpcContext* grpc_context, const Allocator& allocator) noexcept
        : Base(grpc_context, allocator)
    {
        grpc_context->work_started();
    }
};

#if !defined(AGRPC_UNIFEX) && !defined(AGRPC_STDEXEC)
template <bool IsBlockingNever>
struct QueryStaticBlocking
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    [[nodiscard]] static constexpr auto value() noexcept
    {
        if constexpr (IsBlockingNever)
        {
            return asio::execution::blocking_t::never;
        }
        else
        {
            return asio::execution::blocking_t::possibly;
        }
    }

    using result_type = decltype(QueryStaticBlocking::value());
};

template <bool IsWorkTracked>
struct QueryStaticWorkTracked
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    [[nodiscard]] static constexpr auto value() noexcept
    {
        if constexpr (IsWorkTracked)
        {
            return asio::execution::outstanding_work_t::tracked;
        }
        else
        {
            return asio::execution::outstanding_work_t::untracked;
        }
    }

    using result_type = decltype(QueryStaticWorkTracked::value());
};

struct QueryStaticMapping
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::asio::execution::mapping_t::thread_t;

    [[nodiscard]] static constexpr auto value() noexcept { return result_type(); }
};

struct QueryStaticRelationship
{
    static constexpr bool is_valid = true;
    static constexpr bool is_noexcept = true;

    using result_type = agrpc::asio::execution::relationship_t::fork_t;

    [[nodiscard]] static constexpr auto value() noexcept { return result_type(); }
};
#endif
}

AGRPC_NAMESPACE_END

#endif  // AGRPC_DETAIL_GRPC_EXECUTOR_BASE_HPP
