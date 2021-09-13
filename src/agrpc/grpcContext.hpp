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

#ifndef AGRPC_AGRPC_GRPCCONTEXT_HPP
#define AGRPC_AGRPC_GRPCCONTEXT_HPP

#include "agrpc/detail/asioForward.hpp"
#include "agrpc/detail/grpcContextImplementation.hpp"
#include "agrpc/detail/grpcExecutorOptions.hpp"
#include "agrpc/detail/memory.hpp"
#include "agrpc/detail/typeErasedOperation.hpp"

#include <boost/intrusive/slist.hpp>
#include <boost/lockfree/queue.hpp>
#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#include <atomic>
#include <memory_resource>
#include <thread>

namespace agrpc
{
template <class Allocator, std::uint32_t Options>
class BasicGrpcExecutor;

class GrpcContext : public asio::execution_context
{
  private:
    using LocalMemoryResource = std::pmr::unsynchronized_pool_resource;

  public:
    using executor_type = agrpc::BasicGrpcExecutor<std::allocator<void>, detail::GrpcExecutorOptions::DEFAULT>;
    using allocator_type = detail::MemoryResourceAllocator<std::byte, LocalMemoryResource>;

    explicit GrpcContext(std::unique_ptr<grpc::CompletionQueue> completion_queue,
                         std::pmr::memory_resource* local_upstream_resource = std::pmr::new_delete_resource());

    ~GrpcContext();

    void run();

    void stop();

    void reset() noexcept;

    [[nodiscard]] bool is_stopped() const noexcept;

    [[nodiscard]] executor_type get_executor() noexcept;

    [[nodiscard]] allocator_type get_allocator() noexcept;

    void work_started() noexcept;

    void work_finished() noexcept;

    [[nodiscard]] grpc::CompletionQueue* get_completion_queue() noexcept;

    [[nodiscard]] grpc::ServerCompletionQueue* get_server_completion_queue() noexcept;

  private:
    using RemoteWorkQueue = boost::lockfree::queue<detail::TypeErasedNoArgOperation*>;
    using LocalWorkQueue =
        boost::intrusive::slist<detail::ListableTypeErasedNoArgOperation, boost::intrusive::constant_time_size<false>,
                                boost::intrusive::cache_last<true>>;

    grpc::Alarm work_alarm;
    std::atomic_long outstanding_work;
    std::atomic<std::thread::id> thread_id;
    std::atomic_bool stopped;
    std::atomic_bool has_work;
    std::unique_ptr<grpc::CompletionQueue> completion_queue;
    LocalMemoryResource local_resource;
    LocalWorkQueue local_work_queue;
    bool is_processing_local_work;
    RemoteWorkQueue remote_work_queue;

    friend detail::GrpcContextImplementation;
};
}  // namespace agrpc

#endif  // AGRPC_AGRPC_GRPCCONTEXT_HPP
