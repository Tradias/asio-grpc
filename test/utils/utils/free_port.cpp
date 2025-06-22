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

#include "utils/free_port.hpp"

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>

namespace test
{
namespace
{
namespace fs = std::filesystem;

constexpr uint16_t START_PORT = 16397u;
constexpr auto MAX_PORT_FILE_AGE = std::chrono::minutes(1);

template <class Function>
auto perform_under_global_lock(Function&& function)
{
    static std::mutex function_mutex{};
    std::lock_guard function_lock{function_mutex};
    return std::forward<Function>(function)();
}

auto get_port_file_path() { return fs::temp_directory_path() / "agrpcServerUsedTestPort"; }

auto get_port_lock_file_path()
{
    auto path = get_port_file_path();
    path += ".lock";
    return path;
}

void recreate_if_old(const fs::path& port_file, std::chrono::nanoseconds max_age)
{
    if (fs::exists(port_file))
    {
        const auto last_write_time = fs::last_write_time(port_file);
        if (last_write_time + max_age < std::filesystem::file_time_type::clock::now())
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

void create_file_if_not_exist(const fs::path& path)
{
    if (!fs::exists(path))
    {
        std::ofstream create_file{path.native()};
    }
}

template <class Function>
auto perform_under_file_lock(Function&& function)
{
    static boost::interprocess::file_lock file_lock{get_port_lock_file_path().string().c_str()};
    boost::interprocess::scoped_lock<boost::interprocess::file_lock> scoped{file_lock};
    return std::forward<Function>(function)();
}

auto read_and_increment_port(const fs::path& port_file, uint16_t start_port)
{
    std::fstream file_stream{port_file.native(), std::ios::in | std::ios::out};

    static constexpr auto MAX_DIGITS = std::numeric_limits<uint16_t>::digits10 + 1;
    std::array<char, MAX_DIGITS> file_content{};
    file_stream.read(file_content.data(), file_content.size());

    uint16_t port = start_port;
    std::from_chars(file_content.data(), file_content.data() + file_content.size(), port);
    ++port;
    const auto [end, _] = std::to_chars(file_content.data(), file_content.data() + file_content.size(), port);

    file_stream.clear();
    file_stream.seekp(std::ios::beg);
    file_stream.write(file_content.data(), end - file_content.data());

    return port;
}
}  // namespace

uint16_t get_free_port()
{
    return perform_under_global_lock(
        []
        {
            const auto port_lock_file = get_port_lock_file_path();
            create_file_if_not_exist(port_lock_file);
            const auto port_file = get_port_file_path();
            return perform_under_file_lock(
                [&]
                {
                    recreate_if_old(port_file, MAX_PORT_FILE_AGE);
                    return read_and_increment_port(port_file, START_PORT);
                });
        });
}
}  // namespace test
