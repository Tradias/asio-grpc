# Client rpc cheat sheet

@tableofcontents

The code below is based on [example.proto](https://github.com/Tradias/asio-grpc/blob/d4bdcc0a06389127bb649ae4ea68185b928a5264/example/proto/example/v1/example.proto).

A single-threaded gRPC client:

@snippet client.cpp client-main-cheat-sheet

## Unary rpc

@snippet client_rpc.cpp client-rpc-unary

## Client-streaming rpc

@snippet client_rpc.cpp client-rpc-client-streaming

## Server-streaming rpc

@snippet client_rpc.cpp client-rpc-server-streaming

## Bidirectional-streaming rpc

@snippet client_rpc.cpp client-rpc-bidirectional-streaming
