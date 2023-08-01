// Copyright 2023 Dennis Hezel
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

#include "utils/free_port.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>

namespace test
{
namespace
{
namespace fs = boost::filesystem;

constexpr auto PORT_FILE_NAME = "agrpcServerUsedTestPort";
constexpr uint16_t START_PORT = 16397u;

void recreate_if_old(const fs::path& port_file)
{
    if (fs::exists(port_file))
    {
        const auto last_write_time = std::chrono::system_clock::from_time_t(fs::last_write_time(port_file));
        if (last_write_time + std::chrono::minutes(1) < std::chrono::system_clock::now())
        {
            fs::remove(port_file);
            std::ofstream create_file{port_file.native()};
        }
    }
    else
    {
        std::ofstream create_file{port_file.native()};
    }
}

auto get_port_lock_file()
{
    auto port_file = fs::temp_directory_path() / (std::string(PORT_FILE_NAME) + ".lock");
    std::ofstream file_stream{port_file.native()};
    return port_file;
}

template <class Function>
auto perform_under_file_lock(Function&& function, const fs::path& lock_file)
{
    auto lock_file_name = lock_file.string();
    static boost::interprocess::file_lock file_lock{lock_file_name.c_str()};
    boost::interprocess::scoped_lock<boost::interprocess::file_lock> scoped{file_lock};
    return std::forward<Function>(function)();
}

auto read_and_increment_port(const fs::path& port_file)
{
    uint16_t port = START_PORT;
    std::fstream file_stream{port_file.native(), std::ios::in | std::ios::out};
    static constexpr auto MAX_DIGITS = std::numeric_limits<decltype(port)>::digits10 + 1;
    std::array<char, MAX_DIGITS> file_content{};
    file_stream.read(file_content.data(), file_content.size());

    std::from_chars(file_content.data(), file_content.data() + file_content.size(), port);
    ++port;
    const auto [end, _] = std::to_chars(file_content.data(), file_content.data() + file_content.size(), port);

    file_stream.clear();
    file_stream.seekp(std::ios::beg);
    file_stream.write(file_content.data(), end - file_content.data());
    return port;
}

template <class Function>
auto perform_under_mutex_lock(Function&& function)
{
    static std::mutex function_mutex{};
    std::lock_guard function_lock{function_mutex};
    return std::forward<Function>(function)();
}
}  // namespace

uint16_t get_free_port()
{
    return perform_under_mutex_lock(
        []
        {
            static auto port_lock_file = get_port_lock_file();
            const auto port_file = fs::temp_directory_path() / PORT_FILE_NAME;
            return perform_under_file_lock(
                [&]
                {
                    recreate_if_old(port_file);
                    return read_and_increment_port(port_file);
                },
                port_lock_file);
        });
}
}  // namespace test
