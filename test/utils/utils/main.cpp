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

#include <gmock/gmock.h>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int main(int argc, char** argv)
{
    auto& listeners = testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());

    class DoctestListener : public testing::EmptyTestEventListener
    {
      public:
        virtual void OnTestPartResult(const testing::TestPartResult& result) override
        {
            if (!result.failed())
            {
                return;
            }
            const char* file = result.file_name() ? result.file_name() : "unknown";
            const int line = result.line_number() != -1 ? result.line_number() : 0;
            const char* message = result.message() ? result.message() : "no message";
            if (result.nonfatally_failed())
            {
                ADD_FAIL_CHECK_AT(file, line, message);
            }
            else
            {
                ADD_FAIL_AT(file, line, message);
            }
        }
    };
    listeners.Append(new DoctestListener);

    return doctest::Context(argc, argv).run();
}
