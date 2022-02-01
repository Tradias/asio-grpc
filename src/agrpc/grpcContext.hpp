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

#ifndef AGRPC_AGRPC_GRPCCONTEXT_HPP
#define AGRPC_AGRPC_GRPCCONTEXT_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/atomicIntrusiveQueue.hpp"
#include "agrpc/detail/config.hpp"
#include "agrpc/detail/grpcContext.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/grpcExecutorOptions.hpp"
#include "agrpc/detail/intrusiveQueue.hpp"
#include "agrpc/detail/memoryResource.hpp"
#include "agrpc/detail/typeErasedOperation.hpp"

#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#include <atomic>
#include <thread>

AGRPC_NAMESPACE_BEGIN()

template <class Allocator, std::uint32_t Options>
class BasicGrpcExecutor;

class GrpcContext
#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
    : public asio::execution_context
#endif
{
  public:
    using executor_type = agrpc::BasicGrpcExecutor<std::allocator<void>, detail::GrpcExecutorOptions::DEFAULT>;
    using allocator_type = detail::GrpcContextLocalAllocator;

    explicit GrpcContext(std::unique_ptr<grpc::CompletionQueue> completion_queue);

    ~GrpcContext();

    void run();

    void stop();

    void reset() noexcept;

    [[nodiscard]] bool is_stopped() const noexcept;

    [[nodiscard]] executor_type get_executor() noexcept;

    [[nodiscard]] executor_type get_scheduler() noexcept;

    [[nodiscard]] allocator_type get_allocator() noexcept;

    void work_started() noexcept;

    void work_finished() noexcept;

    [[nodiscard]] grpc::CompletionQueue* get_completion_queue() noexcept;

    [[nodiscard]] grpc::ServerCompletionQueue* get_server_completion_queue() noexcept;

  private:
    using RemoteWorkQueue = detail::AtomicIntrusiveQueue<detail::TypeErasedNoArgOperation>;
    using LocalWorkQueue = detail::IntrusiveQueue<detail::TypeErasedNoArgOperation>;

    friend detail::GrpcContextImplementation;

    grpc::Alarm work_alarm;
    std::atomic_long outstanding_work{};
    std::atomic_bool stopped{false};
    bool check_remote_work{false};
    std::unique_ptr<grpc::CompletionQueue> completion_queue;
    detail::GrpcContextLocalMemoryResource local_resource{detail::pmr::new_delete_resource()};
    LocalWorkQueue local_work_queue;
    RemoteWorkQueue remote_work_queue{false};
};

AGRPC_NAMESPACE_END

#endif  // AGRPC_AGRPC_GRPCCONTEXT_HPP

#include "agrpc/detail/grpcContext.ipp"