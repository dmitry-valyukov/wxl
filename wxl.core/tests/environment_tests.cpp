#include <gtest/gtest.h>

import std;
import wxl.core;

using wxl::core::environment;

TEST(EnvironmentTest, ProcessorCountIsPositive) {
    EXPECT_GT(environment::processor_count(), 0u);
}

// "The folder the executable lives in" has one check that cannot be fooled by
// a working directory: the test binary itself is found there.
TEST(EnvironmentTest, ApplicationFolderIsWhereTheBinaryLives) {
    const std::filesystem::path folder = environment::application_folder();

    EXPECT_TRUE(folder.is_absolute());
    EXPECT_TRUE(std::filesystem::is_directory(folder));
    EXPECT_TRUE(std::filesystem::exists(folder / L"wxl.core.tests.exe"));
}

// The command line split the way the shell splits it: the program comes first, and it is
// this very binary -- however ctest or a hand launched it, with whatever arguments.
TEST(EnvironmentTest, CommandLineStartsWithTheProgram) {
    const auto arguments = environment::command_line();

    ASSERT_FALSE(arguments.empty());
    const std::filesystem::path program{std::wstring_view{arguments.front()}};
    EXPECT_EQ(program.filename(), L"wxl.core.tests.exe") << program;
    EXPECT_EQ(program.parent_path(), environment::application_folder()) << program;
}
