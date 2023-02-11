# Using Asio io_context

Most applications that use [asio::io_context](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/io_context.html) want to handle all asynchronous operations with it. Unfortunately, the gRPC API makes that impossible. The alternatives with their upsides and downsides are described below.

## Implicit io_context

Since a GrpcContext is also an [asio::execution_context](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/execution_context.html) it supports Asio's [Service](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/reference/Service.html) mechanism. The following code will therefore implicitly create an io_context, a background thread, run the io_context on that thread and post the completion of `async_wait` onto the GrpcContext where the lambda is being invoked.

@snippet{code} io_context.cpp implicit_io_context

While this is the most convenient approach is also has some downsides:

* The io_context cannot be run on more than one thread.
* There is runtime overhead due to uncontrollable thread switching.

## Explicit io_context

GrpcContext and io_context can also be created directly and used as usual: submit work and run. It is often convenient to declare one of them as the "main" context, like the io_context in the example below.

@snippet{code} io_context.cpp co_spawn_io_context_and_grpc_context

For running the contexts there are two choices. Either on separate threads:

@snippet{code} io_context.cpp run_io_context_separate_thread

Or on the same thread. Here the work counting of the io_context is aligned with that of the GrpcContext:

@snippet{code} io_context.cpp agrpc_run_io_context_shared_work

Note that both approaches come with their own different kind of overheads. Running on two different threads might require additional synchronization in the user code while running on the same thread reduces peak performance. In the [Performance](https://github.com/Tradias/asio-grpc#performance) section of the README you can find results for using an idle io_context with a busy GrpcContext running on the same thread (look for `cpp_asio_grpc_io_context_coro`). Your mileage might vary, please always measure!

