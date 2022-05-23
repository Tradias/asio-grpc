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

#include <gtest/gtest.h>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

class DoctestListener : public testing::EmptyTestEventListener
{
  public:
    virtual void OnTestPartResult(const testing::TestPartResult& test_part_result) override
    {
        if (!test_part_result.failed())
        {
            return;
        }
        const auto file_name = test_part_result.file_name();
        const auto line = test_part_result.line_number();
        const auto message = test_part_result.message();
        const auto file = file_name ? file_name : "[unknown file]";
        if (test_part_result.nonfatally_failed())
        {
            ADD_FAIL_CHECK_AT(file, line, message);
        }
        else if (test_part_result.fatally_failed())
        {
            ADD_FAIL_AT(file, line, message);
        }
    }
};

int main(int argc, char** argv)
{
    auto& listeners = testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new DoctestListener);
    return doctest::Context(argc, argv).run();
}
