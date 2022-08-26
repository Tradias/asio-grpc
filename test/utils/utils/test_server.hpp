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

#ifndef AGRPC_UTILS_TEST_SERVER_HPP
#define AGRPC_UTILS_TEST_SERVER_HPP

#include "test/v1/test.grpc.pb.h"

#include <agrpc/rpc.hpp>

namespace test
{
template <auto Request>
struct TestServer;

template <>
struct TestServer<&test::v1::Test::AsyncService::RequestUnary>
{
    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return agrpc::request(&test::v1::Test::AsyncService::RequestUnary, this->service, this->server_context,
                              this->request, this->responder, std::forward<CompletionToken>(token));
    }

    test::v1::Test::AsyncService& service;
    grpc::ServerContext& server_context;
    test::msg::Request request{};
    test::msg::Response response{};
    grpc::ServerAsyncResponseWriter<test::msg::Response> responder{&server_context};
};

template <>
struct TestServer<&test::v1::Test::AsyncService::RequestClientStreaming>
{
    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return agrpc::request(&test::v1::Test::AsyncService::RequestClientStreaming, this->service,
                              this->server_context, this->responder, std::forward<CompletionToken>(token));
    }

    test::v1::Test::AsyncService& service;
    grpc::ServerContext& server_context;
    test::msg::Request request{};
    test::msg::Response response{};
    grpc::ServerAsyncReader<test::msg::Response, test::msg::Request> responder{&server_context};
};

template <>
struct TestServer<&test::v1::Test::AsyncService::RequestServerStreaming>
{
    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return agrpc::request(&test::v1::Test::AsyncService::RequestServerStreaming, this->service,
                              this->server_context, this->request, this->responder,
                              std::forward<CompletionToken>(token));
    }

    test::v1::Test::AsyncService& service;
    grpc::ServerContext& server_context;
    test::msg::Request request{};
    test::msg::Response response{};
    grpc::ServerAsyncWriter<test::msg::Response> responder{&server_context};
};

template <>
struct TestServer<&test::v1::Test::AsyncService::RequestBidirectionalStreaming>
{
    template <class CompletionToken>
    auto request_rpc(CompletionToken&& token)
    {
        return agrpc::request(&test::v1::Test::AsyncService::RequestBidirectionalStreaming, this->service,
                              this->server_context, this->responder, std::forward<CompletionToken>(token));
    }

    test::v1::Test::AsyncService& service;
    grpc::ServerContext& server_context;
    test::msg::Request request{};
    test::msg::Response response{};
    grpc::ServerAsyncReaderWriter<test::msg::Response, test::msg::Request> responder{&server_context};
};
}

#endif  // AGRPC_UTILS_TEST_SERVER_HPP
