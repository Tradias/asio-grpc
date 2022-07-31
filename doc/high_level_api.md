# High-level API design

This API has not been implemented yet.

Considerations:

* Decent default allocation strategy, especially for users of coroutines
* Support `googe::protobuf::Arena`
* Automatic cancellation on destruction without prior call to finish
* Compose with `agrpc::CancelSafe`
* Compose with async coroutine generators like `asio::experimental::coroutine` and `std::generator`
* Support sender/receiver

Table of contents

* [Proposal 1](#proposal-1)
* [Proposal 2](#proposal-2)

# Proposal 1

## Client-side unary

### API

```cpp
// It might be possible to have just one type for all client-side RPCs but then we need to make assumptions about the concrete
// Responder types. We would only be able to support ClientAsyncResponseReader and ClientAsyncResponseReaderInterface
// in that case. Maybe that is good enough?
template<auto Rpc, class Executor>
class UnaryRpc;

template<class Stub, class RequestT, template<class> class ResponseWriter, class ResponseT,
    std::unique_ptr<ResponseWriter<ResponseT>>(Stub::*Async)(grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*),
    class Executor>
class UnaryRpc<Async, Executor> {
public:
  using Request = RequestT;
  using Response = ResponseT;

  UnaryRpc(Stub&, Executor);

  // Requests and finishes the RPC.
  // Completes with grpc::Status.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto finish(const Request& request, Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  grpc::ClientContext& context() noexcept;

private:
  Stub& stub;
  Executor executor;
  grpc::ClientContext context;
  std::unique_ptr<ResponseWriter<ResponseT>> responder;
  grpc::Status status; // moved on finish
};
```

### Usage

```cpp
using Unary = agrpc::UnaryRpc<&Stub::AsyncUnary, agrpc::GrpcExecutor>;
// Can bind default completion token
//using Unary = asio::use_awaitable_t<>::as_default_on_t<agrpc::UnaryRpc<&Stub::AsyncUnary, agrpc::GrpcExecutor>>;

// Users can store `Unary` on the heap for callback-based code.
Unary unary{stub, grpc_context};
unary.context().set_deadline(...);

Unary::Response response;
// Users can use Arena.
//auto& response = *google::protobuf::Arena::CreateMessage<Unary::Response>(&arena);

grpc::Status status = co_await unary.finish(Unary::Request{}, response, asio::use_awaitable);
if (status.ok()) {
  // Use response.
}
```

## Client-side server-streaming

### API

```cpp
template<auto Rpc, class Executor>
class StreamingRpc;

template<class Stub, class RequestT, template<class> class Reader, class ResponseT,
    std::unique_ptr<Reader<ResponseT>>(Stub::*PrepareAsync)(grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*),
    class Executor>
class StreamingRpc<PrepareAsync, Executor> {
public:
  using Request = RequestT;
  using Response = ResponseT;

  StreamingRpc(Stub&, Executor);

  // Automatically call ClientContext::TryCancel() if not finished
  ~StreamingRpc();

  // Requests the RPC and finishes it if agrpc::request returned `false`.
  // Returns immediately if ClientContext.initial_metadata_corked is set.
  // Completes with grpc::Status.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto start(const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started, or should we just assert?
  // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Reads from the RPC and finishes it if agrpc::read returned `false`.
  // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
  // and the successful end of the stream.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read(Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  grpc::ClientContext& context() noexcept;

private:
  Stub& stub;
  Executor executor;
  grpc::ClientContext context;
  std::unique_ptr<Reader<ResponseT>> responder;
  grpc::Status status;
};
```

### Usage

```cpp
using Streaming = agrpc::StreamingRpc<&Stub::PrepareAsyncServerStreaming, agrpc::GrpcExecutor>;
Streaming streaming{stub, grpc_context};

grpc::Status status = co_await streaming.start(Streaming::Request{}, asio::use_awaitable);
if (!status.ok()) {
  // Server is unreachable, example:
  // UNAVAILABLE (14) "failed to connect to all addresses; last error: UNAVAILABLE: WSA Error"
}

Streaming::Response response;
// Users can use Arena.
//auto& response = *google::protobuf::Arena::CreateMessage<Unary::Response>(&arena);

agrpc::ReadStatus status;
// Should we really promote assignment in conditions?
// Also, inconsistency between boolean-test and .ok()
// Boolean-test true means that we got a response, false that we reached the end of the stream.
while (status = co_await streaming.read(response, asio::use_awaitable)) {
  // Use response.
} 
if (!status.ok()) {
  // agrpc::read returned false and agrpc::finish produced non-ok status
}
```

## Client-side client-streaming

### API

```cpp
template<class Stub, class RequestT, template<class> class Writer, class ResponseT,
    std::unique_ptr<Writer<RequestT>>(Stub::*PrepareAsync)(grpc::ClientContext*, ResponseT*, grpc::CompletionQueue*),
    class Executor>
class StreamingRpc<PrepareAsync, Executor> {
public:
  using Request = RequestT;
  using Response = ResponseT;

  StreamingRpc(Stub&, Executor);

  // Automatically call ClientContext::TryCancel() if not finished
  ~StreamingRpc();

  // Requests the RPC and finishes it if agrpc::request returned `false`.
  // Return immediately if ClientContext.initial_metadata_corked is set.
  // Completes with grpc::Status.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto start(Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  // Completes with grpc::Status.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Reads from the RPC and finishes it if agrpc::write returned `false`.
  // Completes with grpc::Status.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto write(const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
  // Completes with grpc::Status.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto write(const Request& request, grpc::WriteOptions options, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
  // Completes with grpc::Status.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto finish(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  grpc::ClientContext& context() noexcept;

private:
  Stub& stub;
  Executor executor;
  grpc::ClientContext context;
  std::unique_ptr<Writer<RequestT>> responder;
  grpc::Status status;
  bool is_writes_done;
};
```

### Usage

```cpp
using Streaming = agrpc::StreamingRpc<&Stub::PrepareAsyncClientStreaming, agrpc::GrpcExecutor>;
Streaming streaming{stub, grpc_context};

Streaming::Response response;

grpc::Status status = co_await streaming.start(response, asio::use_awaitable);
if (!status.ok()) {
  // Server is unreachable, example:
  // UNAVAILABLE (14) "failed to connect to all addresses; last error: UNAVAILABLE: WSA Error"
}

Streaming::Request request;
while (fill_request(request)) {
  if (status = co_await streaming.write(request, asio::use_awaitable); !status.ok()) {
    // agrpc::write returned false, therefore agrpc::finish produced non-ok status.
    // No reason to call streaming.finish but it should be save to do so.
    co_return;
  }
}

if (status = co_await streaming.finish(asio::use_awaitable); status.ok()) {
  // Use response.
} else {
  // Server finished the stream with non-ok status or the connection got interrupted.
}
```


## Client-side bidirectional-streaming

### API

```cpp
template<class Stub, class RequestT, template<class, class> class ReaderWriter, class ResponseT,
    std::unique_ptr<ReaderWriter<RequestT, ResponseT>>(Stub::*PrepareAsync)(grpc::ClientContext*, grpc::CompletionQueue*),
    class Executor>
class StreamingRpc<PrepareAsync, Executor> {
public:
  using Request = RequestT;
  using Response = ResponseT;

  StreamingRpc(Stub&, Executor);

  // Automatically call ClientContext::TryCancel() if not finished
  ~StreamingRpc();

  // Requests the RPC and finishes it if agrpc::request returned `false`.
  // Return immediately if ClientContext.initial_metadata_corked is set.
  // Completes with grpc::Status.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto start(Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  // Completes with grpc::Status.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Reads from the RPC
  // Should probably complete with the same status type as for server-streaming RPCs, except that the 
  // wrapped grpc::Status will always be OK since this function does not call agrpc::finish. Or maybe 
  // complete with `bool` instead, but then what about FAILED_PRECONDITION?
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read(Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Reads from the RPC and finishes it if agrpc::write returned `false`. Must synchronize with read() before calling finish.
  // Completes with grpc::Status.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto write(const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // WriteOptions::set_last_message() can be used to get the behavior of agrpc::write_last
  // Completes with grpc::Status.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto write(const Request& request, grpc::WriteOptions options, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Calls agrpc::writes_done if not already done by a write with WriteOptions::set_last_message()
  // Completes with grpc::Status.
  // Returns grpc::Status::FAILED_PRECONDITION if the RPC hasn't been started.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto finish(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  grpc::ClientContext& context() noexcept;

private:
  Stub& stub;
  Executor executor;
  grpc::ClientContext context;
  std::unique_ptr<ReaderWriter<RequestT, ResponseT>> responder;
  grpc::Status status;
  bool is_writes_done;
};
```

### Usage

```cpp
using Streaming = agrpc::StreamingRpc<&Stub::PrepareAsyncBidiStreaming, agrpc::GrpcExecutor>;
Streaming streaming{stub, grpc_context};

grpc::Status status = co_await streaming.start(asio::use_awaitable);
if (!status.ok()) {
  // Server is unreachable, example:
  // UNAVAILABLE (14) "failed to connect to all addresses; last error: UNAVAILABLE: WSA Error"
}

Streaming::Request request;
Streaming::Response response;
while (fill_request(request)) {
  auto [status, read_status] = co_await (streaming.write(request, asio::use_awaitable) && streaming.read(response, asio::use_awaitable));
  if (!status.ok()) {
    // agrpc::write returned false, therefore agrpc::finish produced non-ok status
    co_return;
  }
  if (read_status) {
    // Got a response
  }
}

if (status = co_await streaming.finish(asio::use_awaitable); !status.ok()) {
  // Server finished the stream with non-ok status or the connection got interrupted
}
```

# Proposal 2

Prevents the user from interacting with unstarted RPCs and therefore avoids the need for `grpc::Status::FAILED_PRECONDITION`.

## Client-side server-streaming

### API

```cpp
// Should we really try to have one type for all kinds of RPCs?
template<auto Rpc, class Executor>
class Call;

template<class Stub, class RequestT, class ResponseT,
    std::unique_ptr<grpc::ClientAsyncReaderInterface<ResponseT>>(Stub::*PrepareAsync)(grpc::ClientContext*, const RequestT&, grpc::CompletionQueue*),
    class Executor>
class Call<PrepareAsync, Executor> {
public:
  using Request = RequestT;
  using Response = ResponseT;

  // Automatically call ClientContext::TryCancel() if not finished
  ~Call();

  // Completes with grpc::Status::OK if metadata was read, otherwise with what agrpc::finish produced.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read_initial_metadata(CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  // Reads from the RPC and finishes it if agrpc::read returned `false`.
  // Completes with a wrapper around grpc::Status that differentiates between the stati returned from the server
  // and the successful end of the stream.
  template<class CompletionToken = asio::default_completion_token_t<Executor>>
  auto read(Response& response, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

  bool ok() const noexcept { return status_.ok(); }

  grpc::Status& status() noexcept { return status_; }

private:
  Call(Stub&, Executor);

  Stub& stub;
  Executor executor;
  // The user must not move the Call during reads, stub.call() could allocate memory to avoid that. 
  // Would that be helpful?
  std::unique_ptr<grpc::ClientAsyncReaderInterface<ResponseT>> responder;
  grpc::Status status_;
};

template<class StubT, class Executor>
class Stub {
public:
  Stub(StubT&, Executor);

  // Requests the RPC and finishes it if agrpc::request returned `false`.
  // Returns immediately if ClientContext.initial_metadata_corked is set.
  // Completes with some from of `std::expected<Call<Rpc, Executor>, grpc::Status>` or
  // should Call expose grpc::Status' API?
  // Some RPCs are made without an initial request, so we need another overload, is that intuitive?
  // Maybe this function should be called `unary` and then have variations: `client_streaming` and so on.
  template<auto Rpc, class CompletionToken = asio::default_completion_token_t<Executor>>
  auto call(grpc::ClientContext& context, const Request& request, CompletionToken&& token = asio::default_completion_token_t<Executor>{});

private:
  StubT* stub; // Stub could be `void` since .call<auto Rpc>() provides the concrete Stub type for the RPC.
  Executor executor;
};
```

### Usage

```cpp
using Stub = agrpc::Stub<helloworld::Greeter::Stub, agrpc::GrpcExecutor>;
Stub stub{greeter_stub, grpc_context};

// ClientContext and Call are two objects that must stay alive for the duration of the RPC.
// That seems less convenient than the first proposal.
grpc::ClientContext context;
agrpc::Call<&Stub::PrepareAsyncServerStreaming, agrpc::GrpcExecutor> call = 
  co_await stub.template call<&Stub::PrepareAsyncServerStreaming>(context, hellworld::Request{}, asio::use_awaitable);
if (!call.ok()) {
  // Server is unreachable, example:
  // UNAVAILABLE (14) "failed to connect to all addresses; last error: UNAVAILABLE: WSA Error"
}

hellworld::Response response;

agrpc::ReadStatus status;
// Should we really promote assignment in conditions?
// Also, inconsistency between boolean-test and .ok()
// Boolean-test true means that we got a response, false that we reached the end of the stream.
while (status = co_await call.read(response, asio::use_awaitable)) {
  // Use response.
} 
if (!status.ok()) {
  // agrpc::read returned false and agrpc::finish produced non-ok status
}
```

# References

https://github.com/torquem-ch/silkrpc/pull/304   
https://github.com/userver-framework/userver/tree/develop/grpc   
https://github.com/hyperium/tonic/blob/master/examples/routeguide-tutorial.md
