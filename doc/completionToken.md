# Completion token

The last argument to all async functions in this library is a [CompletionToken](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). It can be used to customize how to receive notification of the completion of the asynchronous operation. Some examples:

## Callback

@snippet server.cpp alarm-with-callback

## Stackless coroutine

@snippet server.cpp alarm-stackless-coroutine

## use_sender

In libunifex senders are awaitable. `agrpc::use_sender` causes RPC step functions to return a sender. They can be combined with `unifex::task` to asynchronously process RPCs using `co_await`:

@snippet unifex-client.cpp unifex-server-streaming-client-side

## Custom allocator

Asio-grpc will attempt to get the associated allocator of the completion handler by calling [asio::get_associated_allocator](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/get_associated_allocator.html). If there is none then it will retrieve it from the executor through the [allocator property](https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/reference/execution__allocator_t.html).

The associated allocator can be customized using `agrpc::bind_allocator` (or `asio::bind_allocator` since Boost.Asio 1.79):

@snippet server.cpp alarm-with-allocator-aware-awaitable

The allocator property can be set as follows:

@snippet server.cpp alarm-with-allocator-aware-executor
