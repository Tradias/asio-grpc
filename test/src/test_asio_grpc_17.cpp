// Copyright 2025 Dennis Hezel
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

#include "test/v1/test.grpc.pb.h"
#include "utils/asio_utils.hpp"
#include "utils/doctest.hpp"
#include "utils/grpc_client_server_test.hpp"
#include "utils/grpc_context_test.hpp"
#include "utils/time.hpp"

#include <agrpc/alarm.hpp>
#include <agrpc/detail/algorithm.hpp>
#include <agrpc/notify_on_state_change.hpp>
#include <grpcpp/create_channel.h>

#include <cstddef>
#include <thread>

TEST_CASE("constexpr algorithm: search")
{
    std::string_view text{"find this needle in the haystack"};
    std::string_view needle{"needle"};
    CHECK_EQ(std::search(text.begin(), text.end(), needle.begin(), needle.end()),
             agrpc::detail::search(text.begin(), text.end(), needle.begin(), needle.end()));
}

TEST_CASE("constexpr algorithm: find")
{
    std::string_view text{"find this needle in the haystack"};
    CHECK_EQ(std::find(text.begin(), text.end(), 'y'), agrpc::detail::find(text.begin(), text.end(), 'y'));
}

TEST_CASE("constexpr algorithm: copy")
{
    std::string_view text{"find this needle in the haystack"};
    std::string out(text.size(), '\0');
    const auto copy = agrpc::detail::copy(text.begin(), text.end(), out.begin());
    CHECK_EQ(text, out);
    CHECK_EQ(std::copy(text.begin(), text.end(), out.begin()), copy);
}

TEST_CASE("constexpr algorithm: move")
{
    std::vector<std::unique_ptr<int>> vector;
    vector.emplace_back(std::make_unique<int>(1));
    vector.emplace_back(std::make_unique<int>(2));
    const auto move = agrpc::detail::move(vector.begin(), vector.begin() + 1, vector.begin() + 1);
    CHECK_EQ(vector.end(), move);
    CHECK_EQ(nullptr, vector.front());
    CHECK_EQ(1, *vector.back());
}

TEST_CASE("constexpr algorithm: replace_sequence_with_value")
{
    std::string text{"find this needle in the haystack"};
    std::string_view needle{"needle"};
    const auto new_end = agrpc::detail::replace_sequence_with_value(text.begin(), text.end(), needle, 'x');
    std::string result(text.begin(), new_end);
    CHECK_EQ("find this x in the haystack", result);
}

TEST_CASE_FIXTURE(test::GrpcClientServerTest, "agrpc::notify_on_state_change")
{
    bool actual_ok{false};
    bool expected_ok{true};
    auto deadline = test::five_seconds_from_now();
    grpc_connectivity_state state{};
    SUBCASE("success") { state = channel->GetState(true); }
    SUBCASE("deadline expires")
    {
        actual_ok = true;
        expected_ok = false;
        deadline = test::now();
        state = channel->GetState(false);
    }
    const auto callback = [&](bool ok)
    {
        actual_ok = ok;
    };
    agrpc::notify_on_state_change(grpc_context, *channel, state, deadline, callback);
    grpc_context.run();
    CHECK_EQ(expected_ok, actual_ok);
}

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/yield.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/yield.hpp>
#endif

TEST_CASE_FIXTURE(test::GrpcContextTest, "asio::coroutine with Alarm")
{
    struct Coro : asio::coroutine
    {
        using executor_type = agrpc::GrpcContext::executor_type;

        struct Context
        {
            std::chrono::system_clock::time_point deadline;
            agrpc::GrpcContext& grpc_context;
            bool& ok;
            agrpc::Alarm alarm{grpc_context};

            Context(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context, bool& ok)
                : deadline(deadline), grpc_context(grpc_context), ok(ok)
            {
            }
        };

        std::shared_ptr<Context> context;

        Coro(std::chrono::system_clock::time_point deadline, agrpc::GrpcContext& grpc_context, bool& ok)
            : context(std::make_shared<Context>(deadline, grpc_context, ok))
        {
        }

        void operator()(bool wait_ok)
        {
            reenter(*this)
            {
                yield context->alarm.wait(context->deadline, std::move(*this));
                context->ok = wait_ok;
            }
        }

        executor_type get_executor() const noexcept { return context->grpc_context.get_executor(); }
    };
    bool ok{false};
    Coro{test::ten_milliseconds_from_now(), grpc_context, ok}(false);
    grpc_context.run();
    CHECK(ok);
}

template <class Function>
struct Coro : asio::coroutine
{
    using executor_type = std::decay_t<
        asio::require_result<agrpc::GrpcContext::executor_type, asio::execution::outstanding_work_t::tracked_t>::type>;

    executor_type executor;
    Function function;

    Coro(agrpc::GrpcContext& grpc_context, Function&& f)
        : executor(asio::require(grpc_context.get_executor(), asio::execution::outstanding_work_t::tracked)),
          function(std::forward<Function>(f))
    {
    }

    void operator()(grpc::Status status) { function(status, *this); }

    executor_type get_executor() const noexcept { return executor; }
};

// TEST_CASE_FIXTURE(test::GrpcClientServerTest, "unary stackless coroutine")
// {
//     grpc::ServerAsyncResponseWriter<test::msg::Response> writer{&server_context};
//     test::msg::Request server_request;
//     test::msg::Response server_response;
//     auto server_loop = [&](bool ok, auto& coro) mutable
//     {
//         reenter(coro)
//         {
//             yield test::UnaryClientRPC::request(grpc_context, service, server_context, server_request, writer, coro);
//             CHECK(ok);
//             CHECK_EQ(42, server_request.integer());
//             server_response.set_integer(21);
//             yield agrpc::finish(writer, server_response, grpc::Status::OK, coro);
//             CHECK(ok);
//         }
//     };
//     std::thread server_thread(
//         [&, server_coro = Coro{grpc_context, std::move(server_loop)}]() mutable
//         {
//             server_coro(true);
//         });

//     test::msg::Request client_request;
//     client_request.set_integer(42);
//     test::msg::Response client_response;
//     grpc::Status status;
//     std::unique_ptr<grpc::ClientAsyncResponseReader<test::msg::Response>> reader;
//     auto client_loop = [&](const grpc::Status& status, auto& coro) mutable
//     {
//         reenter(coro)
//         {
//             yield test::UnaryClientRPC::request(coro.get_executor(), *stub, client_context, client_request,
//                                                 client_response, coro);
//             CHECK(status.ok());
//             CHECK_EQ(21, client_response.integer());
//         }
//     };
//     std::thread client_thread(
//         [&, client_coro = Coro{grpc_context, std::move(client_loop)}]() mutable
//         {
//             client_coro(true);
//         });

//     grpc_context.run();
//     server_thread.join();
//     client_thread.join();
// }

#ifdef AGRPC_STANDALONE_ASIO
#include <asio/unyield.hpp>
#elif defined(AGRPC_BOOST_ASIO)
#include <boost/asio/unyield.hpp>
#endif
