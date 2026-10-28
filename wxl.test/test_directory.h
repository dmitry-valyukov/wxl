#pragma once

#include <gtest/gtest.h>
#include <process.h>

import std;
import wxl.core;

namespace wxl::core {

/// %TEMP%\wxl.<suite>.<test>-<process>: a directory the running test has to
/// itself. ctest runs every test as a process of its own and several at once,
/// and another build tree may run the same test at the same moment, so a name
/// fixed per suite, or per test alone, would be shared with one of them. One
/// level under %TEMP%, so a single directory::create makes it.
inline path test_directory() {
    const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string name =
        std::format("wxl.{}.{}-{}", info->test_suite_name(), info->name(), _getpid());

    // The name of a TEST or TEST_F is a C++ identifier, ASCII, so widening it is
    // a plain copy.
    return path(std::filesystem::temp_directory_path().wstring()) /
           std::wstring(name.begin(), name.end());
}

}  // namespace wxl::core
