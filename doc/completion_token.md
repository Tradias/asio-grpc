# Completion token

The last argument to all async functions in this library is a [CompletionToken](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/asynchronous_operations.html#boost_asio.reference.asynchronous_operations.completion_tokens_and_handlers). It can be used to customize how to receive notification of the completion of the asynchronous operation. Some examples:

## Callback

@snippet alarm.cpp alarm-with-callback

## Boost.Coroutine

@snippet alarm.cpp alarm-with-spawn

## use_sender

`agrpc::use_sender` causes functions in this library to return a [Sender](https://github.com/facebookexperimental/libunifex/blob/main/doc/concepts.md#typedsender-concept). They can for example be combined with `unifex::task` to asynchronously process RPCs using `co_await`. Note when using libunifex or stdexec exclusively then `agrpc::use_sender` is already the default completion token:

@snippet unifex_client.cpp unifex-server-streaming-client-side

## Custom allocator

Asio-grpc attempts to get the completion handler's associated allocator by calling [asio::get_associated_allocator](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/get_associated_allocator.html) and uses to allocate intermediate storage, typically for the completion handler itself. Prior to invocation of the completion handler all storage is deallocated.

The associated allocator can be customized using [asio::bind_allocator](https://www.boost.org/doc/libs/1_86_0/doc/html/boost_asio/reference/bind_allocator.html) (since Boost.Asio 1.79):

@snippet alarm.cpp alarm-with-allocator-aware-awaitable
