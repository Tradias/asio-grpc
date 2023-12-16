# Using Asio io_context

@note Due to limitations of the gRPC CompletionQueue and Callback API an [asio::io_context](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/io_context.html) cannot be used to handle RPCs directly. See the end of this document for a detailed explanation.

This article describes how to interoperate between a GrpcContext and an [asio::io_context](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/io_context.html).

## Implicitly constructed io_context

Since a GrpcContext is also an [asio::execution_context](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/execution_context.html) it supports Asio's [Service](https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/Service.html) mechanism. The following code will therefore implicitly create an io_context, a background thread, run the io_context on that thread and post the completion of `async_wait` onto the GrpcContext where the lambda is being invoked.

@snippet{code} io_context.cpp implicit_io_context

Signal_set is just used as an example, it could be any Asio I/O object like `ip::tcp::socket`.

While this is the most convenient approach is also has some downsides:

* The io_context cannot be run on more than one thread.
* There is runtime overhead due to non-customizable thread switching.

## Explicitly constructed io_context

GrpcContext and io_context can also be created directly and used as usual: submit work and run. It is often convenient to utilize one of them as the "main" context. For an example, a gRPC server might use the io_context only for HTTP client operations and the GrpcContext for everything else.

In the following example the io_context is used as the "main" context. When its main coroutine runs to completion, it will signal the GrpcContext to stop  (by releasing the work guard):

@snippet{code} share-io-context-client.cpp co_spawn_io_context_and_grpc_context

For running the contexts there are two choices:

**Run on separate threads**

@snippet{code} io_context.cpp run_io_context_separate_thread

**Run on same thread**

Until the GrpcContext stops:

@snippet{code} io_context.cpp agrpc_run_io_context_and_grpc_context

Or until both contexts stop:

@snippet{code} share-io-context-client.cpp agrpc_run_io_context_shared_work_tracking

### Conclusion

Both approaches come with their own different kind of overheads. Running on two threads might require additional synchronization in the user code while running on the same thread reduces peak performance. In the [Performance](https://github.com/Tradias/asio-grpc#performance) section of the README you can find results for using an idle io_context with a busy GrpcContext running on the same thread (look for `cpp_asio_grpc_io_context_coro`).

## Why io_context cannot be used for gRPC directly

Event loops like the ones used in Asio and gRPC typically utilize system APIs (epoll, IOCompletionPorts, kqueue, ...) in the following order:

1. Create file descriptors for network operations (e.g. sockets and pipes).
2. Initiate some operations on those descriptors (e.g. read and write).
3. Perform a system call (e.g. `poll`) to sleep on ALL descriptors until one or more are ready (e.g. received data).
4. Notify some part of the application, typically by invoking a function pointer.

The important part is to wait on ALL descriptors at once. Which means, for Asio and gRPC to interoperate nicely we would need to collect the descriptors first and then perform the system call to wait. However, file descriptors are created deep in the implementation details of those libraries and the sleep is performed even deeper. GRPC is working on an [EventEngine](https://github.com/grpc/grpc/blob/master/include/grpc/event_engine/README.md) which should make it possible to use Asio sockets for gRPC. Whether it will be enough to fully use Asio for all gRPC network operations remains to be seen.
