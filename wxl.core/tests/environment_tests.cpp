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

// The system's words for a code it knows: a phrase, its line break cut off --
// nor one inside it for this code -- and UTF-8 whatever the language.
TEST(EnvironmentTest, ErrorExplanationIsTheSystemsPhrase) {
    constexpr std::uint32_t file_not_found = 2;  // ERROR_FILE_NOT_FOUND
    const auto words = environment::error_explanation(file_not_found);

    EXPECT_FALSE(words.empty());
    EXPECT_EQ(words.chars().find_first_of("\r\n"), std::string_view::npos) << words.chars();
    EXPECT_FALSE(words.chars().ends_with(' ')) << words.chars();
    EXPECT_TRUE(wxl::core::unicode::checked(words.chars()).has_value()) << words.chars();
}

// A code the system does not know has no explanation: an empty text, not an
// invented phrase.
TEST(EnvironmentTest, AnUnknownCodeHasNoErrorExplanation) {
    EXPECT_TRUE(environment::error_explanation(0x1FFFFFFF).empty());
}

// A file system error as a person reads it: the path as written, then the system's own
// words for the code -- all of it UTF-8, whatever the ANSI code page of the machine.
TEST(EnvironmentTest, ErrorMessageOfAFileSystemErrorIsItsPathAndTheSystemsWords) {
    // "справка", spelled out so that the source's own encoding does not matter.
    const std::filesystem::path file{L"C:\\wxl\\\u0441\u043F\u0440\u0430\u0432\u043A\u0430.xml"};
    constexpr int access_denied = 5;  // ERROR_ACCESS_DENIED
    const std::filesystem::filesystem_error error{"exists", file,
                                                  std::error_code{access_denied, std::system_category()}};

    const auto message = environment::error_message(error);

    const std::string_view name = "C:\\wxl\\\xD1\x81\xD0\xBF\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBA\xD0\xB0.xml: ";
    ASSERT_TRUE(message.chars().starts_with(name)) << message.chars();
    EXPECT_EQ(message.chars().substr(name.size()), environment::error_explanation(access_denied).chars());
    EXPECT_GT(message.chars().size(), name.size()) << message.chars();
}

// Any other error is its own text, and what of it is not UTF-8 comes out marked, not lost.
TEST(EnvironmentTest, ErrorMessageKeepsWhatIsNotUtf8Marked) {
    EXPECT_EQ(environment::error_message(std::runtime_error{"cannot read \xC2\xA0" "file \xFF here"}).chars(),
              "cannot read \xC2\xA0" "file \xEF\xBF\xBD here");
}
