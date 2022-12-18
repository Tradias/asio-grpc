# Examples

Click on `snippet source` to jump to the code of an individual example.

## Hello world server

<!-- snippet: server-side-helloworld -->
<a id='snippet-server-side-helloworld'></a>
```cpp
// ---------------------------------------------------
// Server-side hello world which handles exactly one request from the client before shutting down.
// ---------------------------------------------------
```
<sup><a href='/example/hello-world-server.cpp#L29-L33' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-helloworld' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Generic client

<!-- snippet: client-side-generic-unary-request -->
<a id='snippet-client-side-generic-unary-request'></a>
```cpp
// ---------------------------------------------------
// A simple generic unary with Boost.Coroutine.
// ---------------------------------------------------
```
<sup><a href='/example/generic-client.cpp#L49-L53' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-generic-unary-request' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-generic-bidirectional-request -->
<a id='snippet-client-side-generic-bidirectional-request'></a>
```cpp
// ---------------------------------------------------
// A generic bidirectional-streaming request that simply sends the response from the server back to it.
// Here we are using stackless coroutines and the low-level gRPC client API.
// ---------------------------------------------------
```
<sup><a href='/example/generic-client.cpp#L86-L91' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-generic-bidirectional-request' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Generic server

<!-- snippet: server-side-generic-unary-request -->
<a id='snippet-server-side-generic-unary-request'></a>
```cpp
// ---------------------------------------------------
// Handle a simple generic unary request with Boost.Coroutine.
// ---------------------------------------------------
```
<sup><a href='/example/generic-server.cpp#L39-L43' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-generic-unary-request' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: server-side-generic-bidirectional-request -->
<a id='snippet-server-side-generic-bidirectional-request'></a>
```cpp
// ---------------------------------------------------
// A bidirectional-streaming example that shows how to dispatch requests to a thread_pool and write responses
// back to the client.
// ---------------------------------------------------
```
<sup><a href='/example/generic-server.cpp#L75-L80' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-generic-bidirectional-request' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## io_uring file transfer client

<!-- snippet: client-side-file-transfer -->
<a id='snippet-client-side-file-transfer'></a>
```cpp
// ---------------------------------------------------
// Example showing how to transfer files over a streaming RPC. Only a fixed number of dynamic memory allocations are
// performed.
// ---------------------------------------------------
```
<sup><a href='/example/file-transfer-client.cpp#L36-L41' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-file-transfer' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## io_uring file transfer server

<!-- snippet: server-side-file-transfer -->
<a id='snippet-server-side-file-transfer'></a>
```cpp
// ---------------------------------------------------
// Example showing how to transfer files over a streaming RPC. Only a fixed number of dynamic memory allocations are
// performed.
// ---------------------------------------------------
```
<sup><a href='/example/file-transfer-server.cpp#L38-L43' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-file-transfer' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## High-level client

<!-- snippet: client-side-high-level-client-streaming -->
<a id='snippet-client-side-high-level-client-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple client-streaming request with coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/high-level-client.cpp#L33-L37' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-high-level-client-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-high-level-server-streaming -->
<a id='snippet-client-side-high-level-server-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple server-streaming request with coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/high-level-client.cpp#L70-L74' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-high-level-server-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-high-level-bidirectional-streaming -->
<a id='snippet-client-side-high-level-bidirectional-streaming'></a>
```cpp
// ---------------------------------------------------
// A bidirectional-streaming request that simply sends the response from the server back to it.
// ---------------------------------------------------
```
<sup><a href='/example/high-level-client.cpp#L106-L110' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-high-level-bidirectional-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Multi-threaded client

<!-- snippet: client-side-multi-threaded -->
<a id='snippet-client-side-multi-threaded'></a>
```cpp
// ---------------------------------------------------
// Multi-threaded client performing 20 unary requests
// ---------------------------------------------------
```
<sup><a href='/example/multi-threaded-client.cpp#L31-L35' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-multi-threaded' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: server-side-multi-threaded -->
<a id='snippet-server-side-multi-threaded'></a>
```cpp
// ---------------------------------------------------
// Multi-threaded server performing 20 unary requests
// ---------------------------------------------------
```
<sup><a href='/example/multi-threaded-server.cpp#L31-L35' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-multi-threaded' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Share io_context

<!-- snippet: client-side-share-io-context -->
<a id='snippet-client-side-share-io-context'></a>
```cpp
// ---------------------------------------------------
// Example showing how to run an io_context and a GrpcContext on the same thread for gRPC clients.
// ---------------------------------------------------
```
<sup><a href='/example/share-io-context-client.cpp#L33-L37' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-share-io-context' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: server-side-share-io-context -->
<a id='snippet-server-side-share-io-context'></a>
```cpp
// ---------------------------------------------------
// Example showing how to run an io_context and a GrpcContext on the same thread for gRPC servers.
// This can i.e. be useful when writing an HTTP server that occasionally reaches out to a gRPC server. In that case
// creating a separate thread for the GrpcContext might be undesirable due to added synchronization complexity.
// ---------------------------------------------------
```
<sup><a href='/example/share-io-context-server.cpp#L32-L38' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-share-io-context' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Client-side streaming RPCs

<!-- snippet: client-side-low-level-client-streaming -->
<a id='snippet-client-side-low-level-client-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple client-streaming request with coroutines and the low-level client API.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-client.cpp#L33-L37' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-low-level-client-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-low-level-bidirectional-streaming -->
<a id='snippet-client-side-low-level-bidirectional-streaming'></a>
```cpp
// ---------------------------------------------------
// A bidirectional-streaming request that simply sends the response from the server back to it.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-client.cpp#L72-L76' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-low-level-bidirectional-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-grpc-stream -->
<a id='snippet-client-side-grpc-stream'></a>
```cpp
// ---------------------------------------------------
// -Experimental-
// A bidirectional-streaming RPC where the client subscribes to a topic and the server sends the feed for the last
// subscribed topic every 333ms. The feed is a simple string identified by an integer in the topic.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-client.cpp#L121-L127' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-grpc-stream' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-run-with-deadline -->
<a id='snippet-client-side-run-with-deadline'></a>
```cpp
// ---------------------------------------------------
// A unary request with a per-RPC step timeout. Using a unary RPC for demonstration purposes, the same mechanism can be
// applied to streaming RPCs, where it is arguably more useful.
// For unary RPCs, `grpc::ClientContext::set_deadline` should be preferred.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-client.cpp#L184-L190' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-run-with-deadline' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Server-side streaming RPCs

<!-- snippet: server-side-client-streaming -->
<a id='snippet-server-side-client-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple client-streaming request handler using coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-server.cpp#L37-L41' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-client-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: server-side-server-streaming -->
<a id='snippet-server-side-server-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple server-streaming request handler using coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-server.cpp#L80-L84' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-server-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: server-side-grpc-stream -->
<a id='snippet-server-side-grpc-stream'></a>
```cpp
// ---------------------------------------------------
// -Experimental-
// A bidirectional-streaming RPC where the client subscribes to a topic and the server sends the feed for the last
// subscribed topic every 333ms. The feed is a simple string identified by an integer in the topic.
// ---------------------------------------------------
```
<sup><a href='/example/streaming-server.cpp#L191-L197' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-grpc-stream' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Client-side libunifex

<!-- snippet: client-side-unifex-unary -->
<a id='snippet-client-side-unifex-unary'></a>
```cpp
// ---------------------------------------------------
// A simple unary request with unifex coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/unifex-client.cpp#L32-L36' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-unifex-unary' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-unifex-server-streaming -->
<a id='snippet-client-side-unifex-server-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple server-streaming request with unifex coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/unifex-client.cpp#L54-L58' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-unifex-server-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: client-side-unifex-with-deadline -->
<a id='snippet-client-side-unifex-with-deadline'></a>
```cpp
// ---------------------------------------------------
// A unifex, unary request with a per-RPC step timeout. Using a unary RPC for demonstration purposes, the same mechanism
// can be applied to streaming RPCs, where it is arguably more useful. For unary RPCs,
// `grpc::ClientContext::set_deadline` is the preferred way of specifying a timeout.
// ---------------------------------------------------
```
<sup><a href='/example/unifex-client.cpp#L82-L88' title='Snippet source file'>snippet source</a> | <a href='#snippet-client-side-unifex-with-deadline' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

## Server-side libunifex

<!-- snippet: server-side-unifex-unary -->
<a id='snippet-server-side-unifex-unary'></a>
```cpp
// ---------------------------------------------------
// Register a request handler to unary requests. A bit of boilerplate code regarding stop_source has been added to make
// the example testable.
// ---------------------------------------------------
```
<sup><a href='/example/unifex-server.cpp#L36-L41' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-unifex-unary' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->

<!-- snippet: server-side-unifex-server-streaming -->
<a id='snippet-server-side-unifex-server-streaming'></a>
```cpp
// ---------------------------------------------------
// A simple server-streaming request handler using coroutines.
// ---------------------------------------------------
```
<sup><a href='/example/unifex-server.cpp#L81-L85' title='Snippet source file'>snippet source</a> | <a href='#snippet-server-side-unifex-server-streaming' title='Start of snippet'>anchor</a></sup>
<!-- endSnippet -->
