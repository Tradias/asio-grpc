# Server rpc cheat sheet

@tableofcontents

The code below is based on [example.proto](https://github.com/Tradias/asio-grpc/blob/d4bdcc0a06389127bb649ae4ea68185b928a5264/example/proto/example/v1/example.proto).

A single-threaded gRPC server:

@snippet server.cpp server-main-cheat-sheet

## Unary rpc

@snippet server_rpc.cpp server-rpc-unary

## Client-streaming rpc

@snippet server_rpc.cpp server-rpc-client-streaming

## Server-streaming rpc

@snippet server_rpc.cpp server-rpc-server-streaming

## Bidirectional-streaming rpc

@snippet server_rpc.cpp server-rpc-bidirectional-streaming
