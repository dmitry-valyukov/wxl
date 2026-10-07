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
