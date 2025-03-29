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

#include "helloworld/helloworld.grpc.pb.h"
#include "helper.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <grpc/event_engine/event_engine.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

namespace asio = boost::asio;

namespace gexp = grpc_event_engine::experimental;

inline gexp::EventEngine::ResolvedAddress to_resolved_address(const asio::ip::tcp::endpoint& endpoint)
{
    return {endpoint.data(), static_cast<socklen_t>(endpoint.size())};
}

inline asio::ip::tcp::endpoint to_endpoint(const gexp::EventEngine::ResolvedAddress& address)
{
    asio::ip::tcp::endpoint endpoint;
    endpoint.resize(address.size());
    *endpoint.data() = *address.address();
    return endpoint;
}

class ConstSliceBufferSequence
{
  public:
    class iterator
    {
      public:
        struct value_type
        {
            value_type(asio::const_buffer buffer)
                : slice_(gexp::Slice::FromCopiedBuffer(static_cast<const std::uint8_t*>(buffer.data()), buffer.size()))
            {
            }

            operator asio::const_buffer() const noexcept { return {slice_.data(), slice_.size()}; }

            gexp::Slice slice_;
        };
        using reference = asio::const_buffer;

        iterator() = default;

        iterator(const gexp::SliceBuffer* slice_buffer, std::size_t index) : slice_buffer_(slice_buffer), index_(index)
        {
        }

        reference operator*() const noexcept
        {
            const auto& slice = (*slice_buffer_)[index_];
            return {slice.data(), slice.size()};
        }

        iterator& operator++() noexcept
        {
            ++index_;
            return *this;
        }

        iterator operator++(int) noexcept
        {
            auto copy = *this;
            ++index_;
            return copy;
        }

        bool operator==(const iterator& other) const noexcept
        {
            return slice_buffer_ == other.slice_buffer_ && index_ == other.index_;
        }

        bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

        iterator& operator-() noexcept
        {
            --index_;
            return *this;
        }

        iterator operator--(int) noexcept
        {
            auto copy = *this;
            --index_;
            return copy;
        }

      private:
        const gexp::SliceBuffer* slice_buffer_{};
        std::size_t index_{};
    };

    explicit ConstSliceBufferSequence(const gexp::SliceBuffer& slice_buffer) noexcept : slice_buffer_(slice_buffer) {}

    [[nodiscard]] iterator begin() const noexcept { return {&slice_buffer_, 0}; }

    [[nodiscard]] iterator end() const noexcept
    {
        return {&slice_buffer_, const_cast<gexp::SliceBuffer&>(slice_buffer_).Count()};
    }

  private:
    const gexp::SliceBuffer& slice_buffer_;
};

class AsioEventEngine final : public gexp::EventEngine
{
  private:
    using TimerPtr = std::unique_ptr<asio::steady_timer>;
    using IoContext = asio::io_context;

  public:
    /// A duration between two events.
    ///
    /// Throughout the EventEngine API durations are used to express how long
    /// until an action should be performed.
    // using Duration = std::chrono::duration<int64_t, std::nano>;
    /// A custom closure type for EventEngine task execution.
    ///
    /// Throughout the EventEngine API, \a Closure ownership is retained by the
    /// caller - the EventEngine will never delete a Closure, and upon
    /// cancellation, the EventEngine will simply forget the Closure exists. The
    /// caller is responsible for all necessary cleanup.

    // class Closure
    // {
    //   public:
    //     Closure() = default;
    //     // Closure's are an interface, and thus non-copyable.
    //     Closure(const Closure&) = delete;
    //     Closure& operator=(const Closure&) = delete;
    //     // Polymorphic type => virtual destructor
    //     virtual ~Closure() = default;
    //     // Run the contained code.
    //     virtual void Run() = 0;
    // };

    /// One end of a connection between a gRPC client and server. Endpoints are
    /// created when connections are established, and Endpoint operations are
    /// gRPC's primary means of communication.
    ///
    /// Endpoints must use the provided MemoryAllocator for all data buffer memory
    /// allocations. gRPC allows applications to set memory constraints per
    /// Channel or Server, and the implementation depends on all dynamic memory
    /// allocation being handled by the quota system.
    class Endpoint final : public gexp::EventEngine::Endpoint
    {
      public:
        explicit Endpoint(IoContext& io_context, gexp::MemoryAllocator allocator)
            : socket_(io_context), allocator_(std::move(allocator))
        {
        }

        /// Shuts down all connections and invokes all pending read or write
        /// callbacks with an error status.
        virtual ~Endpoint() = default;
        /// A struct representing optional arguments that may be provided to an
        /// EventEngine Endpoint Read API  call.
        ///
        /// Passed as argument to an Endpoint \a Read
        // struct ReadArgs
        // {
        // A suggestion to the endpoint implementation to read at-least the
        // specified number of bytes over the network connection before marking
        // the endpoint read operation as complete. gRPC may use this argument
        // to minimize the number of endpoint read API calls over the lifetime
        // of a connection.
        //     int64_t read_hint_bytes;
        // };
        /// Reads data from the Endpoint.
        ///
        /// When data is available on the connection, that data is moved into the
        /// \a buffer. If the read succeeds immediately, it returns true and the \a
        /// on_read callback is not executed. Otherwise it returns false and the \a
        /// on_read callback executes asynchronously when the read completes. The
        /// caller must ensure that the callback has access to the buffer when it
        /// executes. Ownership of the buffer is not transferred. Either an error is
        /// passed to the callback (like socket closed), or valid data is available
        /// in the buffer, but never both at the same time. Implementations that
        /// receive valid data must not throw that data away - that is, if valid
        /// data is received on the underlying endpoint, a callback will be made
        /// with that data available and an ok status.
        ///
        /// There can be at most one outstanding read per Endpoint at any given
        /// time. An outstanding read is one in which the \a on_read callback has
        /// not yet been executed for some previous call to \a Read.  If an attempt
        /// is made to call \a Read while a previous read is still outstanding, the
        /// \a EventEngine must abort.
        ///
        /// For failed read operations, implementations should pass the appropriate
        /// statuses to \a on_read. For example, callbacks might expect to receive
        /// CANCELLED on endpoint shutdown.
        virtual bool Read(absl::AnyInvocable<void(absl::Status)> on_read, gexp::SliceBuffer* buffer,
                          const ReadArgs* args)
        {
            auto data = std::make_unique<std::vector<uint8_t>>();
            data->reserve(args->read_hint_bytes);
            auto& vector = *data;
            asio::async_read(
                socket_, asio::dynamic_buffer(vector), asio::transfer_at_least(args->read_hint_bytes),
                [on_read = std::move(on_read), data = std::move(data), buffer](const auto& ec, const auto&) mutable
                {
                    if (ec)
                    {
                        on_read(absl::CancelledError(ec.message()));
                    }
                    else
                    {
                        buffer->Append(gexp::Slice::FromCopiedBuffer(data->data(), data->size()));
                        on_read(absl::OkStatus());
                    }
                });
            return false;
        }

        /// A struct representing optional arguments that may be provided to an
        /// EventEngine Endpoint Write API call.
        ///
        /// Passed as argument to an Endpoint \a Write
        // struct WriteArgs
        // {
        // Represents private information that may be passed by gRPC for
        // select endpoints expected to be used only within google.
        // void* google_specific = nullptr;
        // A suggestion to the endpoint implementation to group data to be written
        // into frames of the specified max_frame_size. gRPC may use this
        // argument to dynamically control the max sizes of frames sent to a
        // receiver in response to high receiver memory pressure.
        //     int64_t max_frame_size;
        // };
        /// Writes data out on the connection.
        ///
        /// If the write succeeds immediately, it returns true and the
        /// \a on_writable callback is not executed. Otherwise it returns false and
        /// the \a on_writable callback is called asynchronously when the connection
        /// is ready for more data. The Slices within the \a data buffer may be
        /// mutated at will by the Endpoint until \a on_writable is called. The \a
        /// data SliceBuffer will remain valid after calling \a Write, but its state
        /// is otherwise undefined.  All bytes in \a data must have been written
        /// before calling \a on_writable unless an error has occurred.
        ///
        /// There can be at most one outstanding write per Endpoint at any given
        /// time. An outstanding write is one in which the \a on_writable callback
        /// has not yet been executed for some previous call to \a Write.  If an
        /// attempt is made to call \a Write while a previous write is still
        /// outstanding, the \a EventEngine must abort.
        ///
        /// For failed write operations, implementations should pass the appropriate
        /// statuses to \a on_writable. For example, callbacks might expect to
        /// receive CANCELLED on endpoint shutdown.
        virtual bool Write(absl::AnyInvocable<void(absl::Status)> on_writable, gexp::SliceBuffer* data,
                           const WriteArgs*)
        {
            asio::async_write(socket_, ConstSliceBufferSequence(*data),
                              [on_writable = std::move(on_writable)](const auto& ec, const auto&) mutable
                              {
                                  if (ec)
                                  {
                                      on_writable(absl::CancelledError(ec.message()));
                                  }
                                  else
                                  {
                                      on_writable(absl::OkStatus());
                                  }
                              });
            return false;
        }

        /// Returns an address in the format described in DNSResolver. The returned
        /// values are expected to remain valid for the life of the Endpoint.
        const ResolvedAddress& GetPeerAddress() const override { return peer_address_; }
        const ResolvedAddress& GetLocalAddress() const override { return local_address_; }

        auto& get_socket() noexcept { return socket_; }

        void populate_addresses()
        {
            peer_address_ = to_resolved_address(socket_.remote_endpoint());
            local_address_ = to_resolved_address(socket_.local_endpoint());
        }

      private:
        asio::ip::tcp::socket socket_;
        gexp::MemoryAllocator allocator_;
        ResolvedAddress peer_address_;
        ResolvedAddress local_address_;
    };

    /// Called when a new connection is established.
    ///
    /// If the connection attempt was not successful, implementations should pass
    /// the appropriate statuses to this callback. For example, callbacks might
    /// expect to receive DEADLINE_EXCEEDED statuses when appropriate, or
    /// CANCELLED statuses on EventEngine shutdown.
    // using OnConnectCallback = absl::AnyInvocable<void(absl::StatusOr<std::unique_ptr<Endpoint>>)>;

    /// Listens for incoming connection requests from gRPC clients and initiates
    /// request processing once connections are established.
    class Listener : public gexp::EventEngine::Listener
    {
      public:
        /// Called when the listener has accepted a new client connection.
        // using AcceptCallback = absl::AnyInvocable<void(std::unique_ptr<Endpoint>, MemoryAllocator memory_allocator)>;
        virtual ~Listener() = default;
        /// Bind an address/port to this Listener.
        ///
        /// It is expected that multiple addresses/ports can be bound to this
        /// Listener before Listener::Start has been called. Returns either the
        /// bound port or an appropriate error status.
        virtual absl::StatusOr<int> Bind(const ResolvedAddress& addr) = 0;
        virtual absl::Status Start() = 0;
    };

    /// Factory method to create a network listener / server.
    ///
    /// Once a \a Listener is created and started, the \a on_accept callback will
    /// be called once asynchronously for each established connection. This method
    /// may return a non-OK status immediately if an error was encountered in any
    /// synchronous steps required to create the Listener. In this case,
    /// \a on_shutdown will never be called.
    ///
    /// If this method returns a Listener, then \a on_shutdown will be invoked
    /// exactly once when the Listener is shut down, and only after all
    /// \a on_accept callbacks have finished executing. The status passed to it
    /// will indicate if there was a problem during shutdown.
    ///
    /// The provided \a MemoryAllocatorFactory is used to create \a
    /// MemoryAllocators for Endpoint construction.
    virtual absl::StatusOr<std::unique_ptr<gexp::EventEngine::Listener>> CreateListener(
        Listener::AcceptCallback on_accept, absl::AnyInvocable<void(absl::Status)> on_shutdown,
        const gexp::EndpointConfig& config, std::unique_ptr<gexp::MemoryAllocatorFactory> memory_allocator_factory)
    {
        return nullptr;
    }
    /// Creates a client network connection to a remote network listener.
    ///
    /// Even in the event of an error, it is expected that the \a on_connect
    /// callback will be asynchronously executed exactly once by the EventEngine.
    /// A connection attempt can be cancelled using the \a CancelConnect method.
    ///
    /// Implementation Note: it is important that the \a memory_allocator be used
    /// for all read/write buffer allocations in the EventEngine implementation.
    /// This allows gRPC's \a ResourceQuota system to monitor and control memory
    /// usage with graceful degradation mechanisms. Please see the \a
    /// MemoryAllocator API for more information.
    virtual ConnectionHandle Connect(OnConnectCallback on_connect, const ResolvedAddress& addr,
                                     const gexp::EndpointConfig& args, gexp::MemoryAllocator memory_allocator,
                                     Duration timeout)
    {
        auto endpoint_ptr = make_endpoint(std::move(memory_allocator));
        auto& socket = endpoint_ptr->get_socket();
        socket.async_connect(
            to_endpoint(addr),
            [endpoint_ptr = std::move(endpoint_ptr), on_connect = std::move(on_connect)](const auto& ec) mutable
            {
                if (ec)
                {
                    on_connect(absl::InternalError(ec.message()));
                }
                else
                {
                    endpoint_ptr->populate_addresses();
                    on_connect(std::move(endpoint_ptr));
                }
            });
        return {reinterpret_cast<intptr_t>(endpoint_ptr.get())};
    }

    /// Request cancellation of a connection attempt.
    ///
    /// If the associated connection has already been completed, it will not be
    /// cancelled, and this method will return false.
    ///
    /// If the associated connection has not been completed, it will be cancelled,
    /// and this method will return true. The \a OnConnectCallback will not be
    /// called, and \a on_connect will be destroyed before this method returns.
    virtual bool CancelConnect(ConnectionHandle handle) { return false; }

    /// Provides asynchronous resolution.
    ///
    /// This object has a destruction-is-cancellation semantic.
    /// Implementations should make sure that all pending requests are cancelled
    /// when the object is destroyed and all pending callbacks will be called
    /// shortly. If cancellation races with request completion, implementations
    /// may choose to either cancel or satisfy the request.
    class DNSResolver final : public gexp::EventEngine::DNSResolver
    {
      public:
        // /// Optional configuration for DNSResolvers.
        // struct ResolverOptions
        // {
        //     /// If empty, default DNS servers will be used.
        //     /// Must be in the "IP:port" format as described in naming.md.
        //     std::string dns_server;
        // };
        // /// DNS SRV record type.
        // struct SRVRecord
        // {
        //     std::string host;
        //     int port = 0;
        //     int priority = 0;
        //     int weight = 0;
        // };
        // /// Called with the collection of sockaddrs that were resolved from a given
        // /// target address.
        // using LookupHostnameCallback = absl::AnyInvocable<void(absl::StatusOr<std::vector<ResolvedAddress>>)>;
        // /// Called with a collection of SRV records.
        // using LookupSRVCallback = absl::AnyInvocable<void(absl::StatusOr<std::vector<SRVRecord>>)>;
        // /// Called with the result of a TXT record lookup
        // using LookupTXTCallback = absl::AnyInvocable<void(absl::StatusOr<std::vector<std::string>>)>;

        explicit DNSResolver(IoContext& io_context) : resolver_(io_context) {}

        virtual ~DNSResolver() = default;

        /// Asynchronously resolve an address.
        ///
        /// \a default_port may be a non-numeric named service port, and will only
        /// be used if \a address does not already contain a port component.
        ///
        /// When the lookup is complete or cancelled, the \a on_resolve callback
        /// will be invoked with a status indicating the success or failure of the
        /// lookup. Implementations should pass the appropriate statuses to the
        /// callback. For example, callbacks might expect to receive CANCELLED or
        /// NOT_FOUND.
        virtual void LookupHostname(LookupHostnameCallback on_resolve, absl::string_view name,
                                    absl::string_view default_port)
        {
            // auto addr = asio::ip::make_address(name);
            const auto [host, port] = split_host_port(name);
            resolver_.async_resolve(
                host, port.empty() ? default_port : port,
                [on_resolve = std::move(on_resolve)](const boost::system::error_code& ec,
                                                     const asio::ip::tcp::resolver::results_type& results) mutable
                {
                    if (ec)
                    {
                        on_resolve(absl::InternalError(ec.message()));
                        return;
                    }
                    std::vector<ResolvedAddress> addresses;
                    addresses.reserve(results.size());
                    for (const auto& result : results)
                    {
                        addresses.emplace_back(result.endpoint().data(),
                                               static_cast<socklen_t>(result.endpoint().size()));
                    }
                    on_resolve(std::move(addresses));
                });
        }
        /// Asynchronously perform an SRV record lookup.
        ///
        /// \a on_resolve has the same meaning and expectations as \a
        /// LookupHostname's \a on_resolve callback.
        virtual void LookupSRV(LookupSRVCallback on_resolve, absl::string_view name) {}
        /// Asynchronously perform a TXT record lookup.
        ///
        /// \a on_resolve has the same meaning and expectations as \a
        /// LookupHostname's \a on_resolve callback.
        virtual void LookupTXT(LookupTXTCallback on_resolve, absl::string_view name) {}

      private:
        asio::ip::tcp::resolver resolver_;
    };

    explicit AsioEventEngine(IoContext& context) : context_(context) {}

    /// At time of destruction, the EventEngine must have no active
    /// responsibilities. EventEngine users (applications) are responsible for
    /// cancelling all tasks and DNS lookups, shutting down listeners and
    /// endpoints, prior to EventEngine destruction. If there are any outstanding
    /// tasks, any running listeners, etc. at time of EventEngine destruction,
    /// that is an invalid use of the API, and it will result in undefined
    /// behavior.
    ~AsioEventEngine() = default;

    // TODO(nnoble): consider whether we can remove this method before we
    // de-experimentalize this API.
    virtual bool IsWorkerThread() { return context_.get_executor().running_in_this_thread(); }

    /// Creates and returns an instance of a DNSResolver, optionally configured by
    /// the \a options struct. This method may return a non-OK status if an error
    /// occurred when creating the DNSResolver. If the caller requests a custom
    /// DNS server, and the EventEngine implementation does not support it, this
    /// must return an error.
    virtual absl::StatusOr<std::unique_ptr<gexp::EventEngine::DNSResolver>> GetDNSResolver(
        const gexp::EventEngine::DNSResolver::ResolverOptions& options)
    {
        if (!options.dns_server.empty())
        {
            return absl::UnimplementedError("Custom DNS server not supported");
        }
        return std::make_unique<DNSResolver>(context_);
    }

    /// Asynchronously executes a task as soon as possible.
    ///
    /// \a Closures passed to \a Run cannot be cancelled. The \a closure will not
    /// be deleted after it has been run, ownership remains with the caller.
    ///
    /// Implementations must not execute the closure in the calling thread before
    /// \a Run returns. For example, if the caller must release a lock before the
    /// closure can proceed, running the closure immediately would cause a
    /// deadlock.
    virtual void Run(Closure* closure)
    {
        asio::post(context_,
                   [closure]
                   {
                       closure->Run();
                   });
    }
    /// Asynchronously executes a task as soon as possible.
    ///
    /// \a Closures passed to \a Run cannot be cancelled. Unlike the overloaded \a
    /// Closure alternative, the absl::AnyInvocable version's \a closure will be
    /// deleted by the EventEngine after the closure has been run.
    ///
    /// This version of \a Run may be less performant than the \a Closure version
    /// in some scenarios. This overload is useful in situations where performance
    /// is not a critical concern.
    ///
    /// Implementations must not execute the closure in the calling thread before
    /// \a Run returns.
    virtual void Run(absl::AnyInvocable<void()> closure)
    {
        asio::post(context_,
                   [closure = std::move(closure)]() mutable
                   {
                       closure();
                   });
    }
    /// Synonymous with scheduling an alarm to run after duration \a when.
    ///
    /// The \a closure will execute when time \a when arrives unless it has been
    /// cancelled via the \a Cancel method. If cancelled, the closure will not be
    /// run, nor will it be deleted. Ownership remains with the caller.
    ///
    /// Implementations must not execute the closure in the calling thread before
    /// \a RunAfter returns.
    ///
    /// Implementations may return a \a kInvalid handle if the callback can be
    /// immediately executed, and is therefore not cancellable.
    virtual TaskHandle RunAfter(Duration when, Closure* closure)
    {
        if (when <= Duration::zero())
        {
            return TaskHandle::kInvalid;
        }
        auto timer = make_timer();
        auto ptr = timer.get();
        timer->expires_after(when);
        timer->async_wait(
            [closure, timer = std::move(timer)](const boost::system::error_code& ec) mutable
            {
                if (!ec)
                {
                    closure->Run();
                }
            });
        return {reinterpret_cast<intptr_t>(ptr)};
    }
    /// Synonymous with scheduling an alarm to run after duration \a when.
    ///
    /// The \a closure will execute when time \a when arrives unless it has been
    /// cancelled via the \a Cancel method. If cancelled, the closure will not be
    /// run. Unlike the overloaded \a Closure alternative, the absl::AnyInvocable
    /// version's \a closure will be deleted by the EventEngine after the closure
    /// has been run, or upon cancellation.
    ///
    /// This version of \a RunAfter may be less performant than the \a Closure
    /// version in some scenarios. This overload is useful in situations where
    /// performance is not a critical concern.
    ///
    /// Implementations must not execute the closure in the calling thread before
    /// \a RunAfter returns.
    virtual TaskHandle RunAfter(Duration when, absl::AnyInvocable<void()> closure)
    {
        auto timer = make_timer();
        auto ptr = timer.get();
        timer->expires_after(when);
        timer->async_wait(
            [closure = std::move(closure), timer = std::move(timer)](const boost::system::error_code& ec) mutable
            {
                if (!ec)
                {
                    closure();
                }
                timer.release();
            });
        return {reinterpret_cast<intptr_t>(ptr)};
    }
    /// Request cancellation of a task.
    ///
    /// If the associated closure cannot be cancelled for any reason, this
    /// function will return false.
    ///
    /// If the associated closure can be cancelled, the associated callback will
    /// never be run, and this method will return true. If the callback type was
    /// an absl::AnyInvocable, it will be destroyed before the method returns.
    virtual bool Cancel(TaskHandle handle)
    {
        auto timer = reinterpret_cast<asio::steady_timer*>(handle.keys[0]);
        return timer->cancel() != 0;
    }

  private:
    TimerPtr make_timer() const { return std::make_unique<asio::steady_timer>(context_); }

    template <class... Args>
    std::unique_ptr<Endpoint> make_endpoint(Args&&... args) const
    {
        return std::make_unique<Endpoint>(context_, static_cast<Args&&>(args)...);
    }

    static std::pair<absl::string_view, absl::string_view> split_host_port(absl::string_view hostname)
    {
        const auto pos = hostname.find_last_of(':');
        return pos == absl::string_view::npos ? std::pair(hostname, absl::string_view{})
                                              : std::pair(hostname.substr(0, pos), hostname.substr(pos + 1));
    }

    IoContext& context_;
};

int main(int argc, const char** argv)
{
    const auto port = argc >= 2 ? argv[1] : "50051";
    const auto host = std::string("127.0.0.1:") + port;

    asio::io_context io_context{1};
    auto guard = asio::make_work_guard(io_context);
    grpc_event_engine::experimental::SetDefaultEventEngine(std::make_shared<AsioEventEngine>(io_context));

    grpc::Status status;

    helloworld::Greeter::Stub stub{grpc::CreateChannel(host, grpc::InsecureChannelCredentials())};

    grpc::ClientContext client_context;
    helloworld::HelloRequest request;
    request.set_name("world");
    helloworld::HelloReply response;
    std::mutex mu;
    std::condition_variable cv;
    bool done = false;
    stub.async()->SayHello(&client_context, &request, &response,
                           [&](const grpc::Status& status)
                           {
                               std::cout << status.error_message() << " response: " << response.message() << std::endl;
                               std::lock_guard<std::mutex> lock(mu);
                               done = true;
                               cv.notify_one();
                           });

    std::thread t{[&]
                  {
                      io_context.run();
                  }};

    std::unique_lock<std::mutex> lock(mu);
    while (!done)
    {
        cv.wait(lock);
    }
    guard.reset();
    t.join();

    abort_if_not(status.ok());
}