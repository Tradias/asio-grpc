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

/*
These are code snippets from the blog article about some of the implementation details of asio-grpc:
https://medium.com/3yourmind/c-20-coroutines-for-asynchronous-grpc-services-5b3dab1d1d61
*/

#include "protos/helloworld.grpc.pb.h"

#include <boost/asio/execution_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <grpcpp/alarm.h>
#include <grpcpp/grpcpp.h>

template <class T, class... Args>
auto allocate(Args&&... args)
{
    return new T{std::forward<Args>(args)...};
}

template <class T>
void deallocate(T* t)
{
    delete t;
}

struct TypeErasedOperation : boost::intrusive::slist_base_hook<>
{
    using OnCompleteFunction = void (*)(TypeErasedOperation*, bool);
    OnCompleteFunction on_complete;

    void complete(bool ok) { this->on_complete(this, ok); }

    TypeErasedOperation(OnCompleteFunction on_complete) : on_complete(on_complete) {}
};

using QueuedOperations = boost::intrusive::slist<TypeErasedOperation>;

template <class Function>
struct Operation : TypeErasedOperation
{
    Function function;

    Operation(Function function) : TypeErasedOperation{&Operation::do_complete}, function(std::move(function)) {}

    static void do_complete(TypeErasedOperation* base, bool ok)
    {
        auto* self = static_cast<Operation*>(base);
        auto func = std::move(self->function);
        deallocate(self);
        func(ok);
    }
};

struct GrpcContext : boost::asio::execution_context
{
    struct executor_type;

    std::unique_ptr<grpc::CompletionQueue> queue;
    QueuedOperations queued_operations;
    grpc::Alarm alarm;

    explicit GrpcContext(std::unique_ptr<grpc::CompletionQueue> queue) : queue(std::move(queue)) {}

    ~GrpcContext()
    {
        queue->Shutdown();
        // drain the queue
        boost::asio::execution_context::shutdown();
        boost::asio::execution_context::destroy();
    }

    executor_type get_executor() const noexcept;

    static constexpr void* MARKER_TAG = nullptr;

    void run()
    {
        void* tag;
        bool ok;
        while (queue->Next(&tag, &ok))
        {
            if (MARKER_TAG == tag)
            {
                while (!queued_operations.empty())
                {
                    auto& op = queued_operations.front();
                    queued_operations.pop_front();
                    op.complete(ok);
                }
            }
            else
            {
                static_cast<TypeErasedOperation*>(tag)->complete(ok);
            }
        }
    }
};

struct GrpcContext::executor_type
{
    GrpcContext* grpc_context{};

    template <class Function>
    void execute(Function function) const
    {
        auto* op = allocate<Operation<Function>>(function);
        grpc_context->queued_operations.push_front(*op);
        grpc_context->alarm.Set(grpc_context->queue.get(), gpr_time_0(GPR_CLOCK_REALTIME), GrpcContext::MARKER_TAG);
    }
};

template <class CompletionToken>
auto read(grpc::ClientAsyncReader<helloworld::HelloReply>& reader, helloworld::HelloReply& reply, CompletionToken token)
{
    return boost::asio::async_initiate<CompletionToken, void(bool)>(
        [&](auto completion_handler)
        {
            void* tag = allocate<Operation<decltype(completion_handler)>>(std::move(completion_handler));
            reader.Read(&reply, tag);
        },
        token);
}

boost::asio::awaitable<void> process_rpc()
{
    std::unique_ptr<grpc::ClientAsyncReader<helloworld::HelloReply>> reader;
    helloworld::HelloReply reply;
    co_await read(*reader, reply, boost::asio::use_awaitable);
}

int main()
{
    GrpcContext grpc_context{std::make_unique<grpc::CompletionQueue>()};
    GrpcContext::executor_type exec{&grpc_context};
    exec.execute([](bool) {});
    grpc_context.run();
}
