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

#include "utils/grpc_context_test.hpp"

#include "utils/asio_forward.hpp"
#include "utils/memory_resource.hpp"

#include <agrpc/grpc_context.hpp>
#include <agrpc/grpc_executor.hpp>
#include <grpc/event_engine/event_engine.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <unifex/execute.hpp>
#include <unifex/with_query_value.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

namespace test
{
namespace
{
using namespace grpc_event_engine::experimental;

struct EvEngine : grpc_event_engine::experimental::EventEngine
{
    class PosixEndpoint : public EventEngine::Endpoint
    {
      public:
        ~PosixEndpoint() override;
        void Read(absl::AnyInvocable<void(absl::Status)> on_read, SliceBuffer* buffer, const ReadArgs* args) override;
        void Write(absl::AnyInvocable<void(absl::Status)> on_writable, SliceBuffer* data,
                   const WriteArgs* args) override;
        const ResolvedAddress& GetPeerAddress() const override;
        const ResolvedAddress& GetLocalAddress() const override;
    };
    class PosixListener : public EventEngine::Listener
    {
      public:
        ~PosixListener() override;
        absl::StatusOr<int> Bind(const ResolvedAddress& addr) override;
        absl::Status Start() override;
    };
    class PosixDNSResolver : public EventEngine::DNSResolver
    {
      public:
        ~PosixDNSResolver() override;
        LookupTaskHandle LookupHostname(LookupHostnameCallback on_resolve, absl::string_view name,
                                        absl::string_view default_port, Duration timeout) override;
        LookupTaskHandle LookupSRV(LookupSRVCallback on_resolve, absl::string_view name, Duration timeout) override;
        LookupTaskHandle LookupTXT(LookupTXTCallback on_resolve, absl::string_view name, Duration timeout) override;
        bool CancelLookup(LookupTaskHandle handle) override;
    };

    unifex::linuxos::io_epoll_context& context;

    explicit EvEngine(unifex::linuxos::io_epoll_context& context) : context(context) {}

    ~EvEngine() override;

    absl::StatusOr<std::unique_ptr<Listener>> CreateListener(
        Listener::AcceptCallback on_accept, absl::AnyInvocable<void(absl::Status)> on_shutdown,
        const EndpointConfig& config, std::unique_ptr<MemoryAllocatorFactory> memory_allocator_factory) override;

    ConnectionHandle Connect(OnConnectCallback on_connect, const ResolvedAddress& addr, const EndpointConfig& args,
                             MemoryAllocator memory_allocator, Duration timeout) override;

    bool CancelConnect(ConnectionHandle handle) override;
    bool IsWorkerThread() override;
    std::unique_ptr<DNSResolver> GetDNSResolver(const DNSResolver::ResolverOptions& options) override;
    void Run(Closure* closure) override;
    void Run(absl::AnyInvocable<void()> closure) override;
    TaskHandle RunAfter(Duration when, Closure* closure) override;
    TaskHandle RunAfter(Duration when, absl::AnyInvocable<void()> closure) override;
    bool Cancel(TaskHandle handle) override;

    struct ClosureData;
    EventEngine::TaskHandle RunAfterInternal(Duration when, absl::AnyInvocable<void()> cb);
};

struct EvEngine::ClosureData final : public EventEngine::Closure
{
    absl::AnyInvocable<void()> cb;
    absl::AnyInvocable<void() noexcept> stop;
    EvEngine* engine;

    void Run() override
    {
        // GRPC_EVENT_ENGINE_TRACE("EvEngine:%p executing callback:%s", engine, HandleToString(handle).c_str());
        // {
        //     grpc_core::MutexLock lock(&engine->mu_);
        //     engine->known_handles_.erase(handle);
        // }
        stop = nullptr;
        cb();
        delete this;
    }
};

struct StopToken
{
    EvEngine::ClosureData* cd;

    template <class F>
    struct callback_type
    {
        callback_type(StopToken token, F f) noexcept { token.cd->stop = std::move(f); }
    };

    static constexpr bool stop_requested() noexcept { return false; }

    static constexpr bool stop_possible() noexcept { return true; }
};

EvEngine::~EvEngine()
{
    // grpc_core::MutexLock lock(&mu_);
    // if (GRPC_TRACE_FLAG_ENABLED(grpc_event_engine_trace))
    // {
    //     for (auto handle : known_handles_)
    //     {
    //         gpr_log(GPR_ERROR,
    //                 "(event_engine) EvEngine:%p uncleared TaskHandle at "
    //                 "shutdown:%s",
    //                 this, HandleToString(handle).c_str());
    //     }
    // }
    // GPR_ASSERT(GPR_LIKELY(known_handles_.empty()));
}

bool EvEngine::Cancel(EventEngine::TaskHandle handle)
{
    // grpc_core::MutexLock lock(&mu_);
    // if (!known_handles_.contains(handle)) return false;
    auto* cd = reinterpret_cast<ClosureData*>(handle.keys[0]);
    if (cd->stop)
    {
        cd->stop();
        cd->stop = nullptr;
        return true;
    }
    // bool r = timer_manager_.TimerCancel(&cd->timer);
    // known_handles_.erase(handle);
    // if (r) delete cd;
    return false;
}

EventEngine::TaskHandle EvEngine::RunAfter(Duration when, absl::AnyInvocable<void()> closure)
{
    return RunAfterInternal(when, std::move(closure));
}

EventEngine::TaskHandle EvEngine::RunAfter(Duration when, EventEngine::Closure* closure)
{
    return RunAfterInternal(when,
                            [closure]()
                            {
                                closure->Run();
                            });
}

void EvEngine::Run(absl::AnyInvocable<void()> closure) { unifex::execute(context.get_scheduler(), std::move(closure)); }

void EvEngine::Run(EventEngine::Closure* closure)
{
    unifex::execute(context.get_scheduler(),
                    [closure]()
                    {
                        closure->Run();
                    });
}

EventEngine::TaskHandle EvEngine::RunAfterInternal(Duration when, absl::AnyInvocable<void()> cb)
{
    struct Receiver
    {
        ClosureData* cd;

        void set_done() noexcept { delete cd; }

        void set_value() { cd->Run(); }

        void set_error(std::exception_ptr) noexcept { delete cd; }
    };
    auto when_ts = context.get_scheduler().now() + when;
    auto* cd = new ClosureData;
    cd->cb = std::move(cb);
    cd->engine = this;
    EventEngine::TaskHandle handle{reinterpret_cast<intptr_t>(cd)};
    // grpc_core::MutexLock lock(&mu_);
    // known_handles_.insert(handle);
    // cd->handle = handle;
    // GRPC_EVENT_ENGINE_TRACE("EvEngine:%p scheduling callback:%s", this, HandleToString(handle).c_str());
    auto s = context.get_scheduler().schedule_at(when_ts);
    unifex::submit(unifex::with_query_value(std::move(s), unifex::get_stop_token, StopToken{cd}),
                   Receiver{std::move(cd)});
    // timer_manager_.TimerInit(&cd->timer, when_ts, cd);
    return handle;
}

std::unique_ptr<EventEngine::DNSResolver> EvEngine::GetDNSResolver(
    EventEngine::DNSResolver::ResolverOptions const& /*options*/)
{
    GPR_ASSERT(false && "unimplemented");
}

bool EvEngine::IsWorkerThread() { GPR_ASSERT(false && "unimplemented"); }

bool EvEngine::CancelConnect(EventEngine::ConnectionHandle /*handle*/) { GPR_ASSERT(false && "unimplemented"); }

EventEngine::ConnectionHandle EvEngine::Connect(OnConnectCallback /*on_connect*/, const ResolvedAddress& /*addr*/,
                                                const EndpointConfig& /*args*/, MemoryAllocator /*memory_allocator*/,
                                                Duration /*deadline*/)
{
    GPR_ASSERT(false && "unimplemented");
}

absl::StatusOr<std::unique_ptr<EventEngine::Listener>> EvEngine::CreateListener(
    Listener::AcceptCallback /*on_accept*/, absl::AnyInvocable<void(absl::Status)> /*on_shutdown*/,
    const EndpointConfig& /*config*/, std::unique_ptr<MemoryAllocatorFactory> /*memory_allocator_factory*/)
{
    GPR_ASSERT(false && "unimplemented");
}
}

bool GrpcContextTest::init()
{
    grpc_event_engine::experimental::SetDefaultEventEngineFactory(
        [&]()
        {
            return std::make_unique<test::EvEngine>(context);
        });
    return true;
}

GrpcContextTest::GrpcContextTest()
    : buffer{}, resource{buffer.data(), buffer.size()}, grpc_context{builder.AddCompletionQueue()}
{
}

agrpc::GrpcExecutor GrpcContextTest::get_executor() noexcept { return grpc_context.get_executor(); }

agrpc::detail::pmr::polymorphic_allocator<std::byte> GrpcContextTest::get_allocator() noexcept
{
    return agrpc::detail::pmr::polymorphic_allocator<std::byte>(&resource);
}

#if defined(AGRPC_STANDALONE_ASIO) || defined(AGRPC_BOOST_ASIO)
agrpc::pmr::GrpcExecutor GrpcContextTest::get_pmr_executor() noexcept
{
    return asio::require(this->get_executor(), asio::execution::allocator(get_allocator()));
}
#endif

bool GrpcContextTest::allocator_has_been_used() noexcept
{
    return std::any_of(buffer.begin(), buffer.end(),
                       [](auto&& value)
                       {
                           return value != std::byte{};
                       });
}
}  // namespace test
