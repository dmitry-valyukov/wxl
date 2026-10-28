// windows.h for SetEnvironmentVariableW: a variable is set in the process's own block,
// where environment::variable() reads it.
#include "platform.h"

#include <gtest/gtest.h>

import std;
import wxl.core;

using wxl::core::environment;

TEST(EnvironmentTest, ProcessorCountIsPositive) { EXPECT_GT(environment::processor_count(), 0u); }

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
    // Compared as paths, element by element: the program may be spelled with / where the
    // folder has \. Shown through to_utf8(), since a path printed by operator<< goes through
    // string(), which throws on a name the ANSI code page cannot write.
    const auto folder = environment::application_folder();
    EXPECT_TRUE(program.filename() == L"wxl.core.tests.exe")
        << wxl::core::unicode::to_utf8(program).chars();
    EXPECT_TRUE(program.parent_path() == folder)
        << wxl::core::unicode::to_utf8(program).chars() << " vs "
        << wxl::core::unicode::to_utf8(folder).chars();
}

// A variable set after the process started -- a folder in Cyrillic, "папка", and a
// character beyond the basic plane, both spelled out so that the source's own encoding
// does not matter -- is read back unit for unit.
TEST(EnvironmentTest, AVariableSetSinceStartupIsReadBack) {
    constexpr wchar_t name[] = L"WXL_CORE_TESTS_VARIABLE";
    const std::wstring value = L"C:\\\u043F\u0430\u043F\u043A\u0430 \U0001F600";
    ASSERT_TRUE(::SetEnvironmentVariableW(name, value.c_str()));

    EXPECT_EQ(environment::variable(name), value);

    ::SetEnvironmentVariableW(name, nullptr);
}

// Longer than any room a first guess would make: the size is asked for, not assumed.
TEST(EnvironmentTest, ALongVariableIsReadWhole) {
    constexpr wchar_t name[] = L"WXL_CORE_TESTS_LONG_VARIABLE";
    const std::wstring value(20000, L'x');
    ASSERT_TRUE(::SetEnvironmentVariableW(name, value.c_str()));

    EXPECT_EQ(environment::variable(name), value);

    ::SetEnvironmentVariableW(name, nullptr);
}

// A variable that is not set reads as empty. The last error is set beforehand to one
// that would throw if it were read as the answer.
TEST(EnvironmentTest, AnAbsentVariableIsEmpty) {
    constexpr wchar_t name[] = L"WXL_CORE_TESTS_ABSENT_VARIABLE";
    ::SetEnvironmentVariableW(name, nullptr);
    ::SetLastError(ERROR_ACCESS_DENIED);

    EXPECT_TRUE(environment::variable(name).empty());
}

// A variable set to nothing reads as empty too, on its own lookup: no earlier one has
// left an error behind, and the one set beforehand, which would throw if it were read
// as the answer, is not.
TEST(EnvironmentTest, AVariableSetToNothingIsEmpty) {
    constexpr wchar_t name[] = L"WXL_CORE_TESTS_EMPTY_VARIABLE";
    ASSERT_TRUE(::SetEnvironmentVariableW(name, L""));
    ::SetLastError(ERROR_ACCESS_DENIED);

    EXPECT_TRUE(environment::variable(name).empty());

    ::SetEnvironmentVariableW(name, nullptr);
}

// The system's words for a code it knows: a phrase, its line break trimmed --
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
    const std::filesystem::filesystem_error error{
        "exists", file, std::error_code{access_denied, std::system_category()}};

    const auto message = environment::error_message(error);

    const std::string_view name =
        "C:\\wxl\\\xD1\x81\xD0\xBF\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBA\xD0\xB0.xml: ";
    ASSERT_TRUE(message.chars().starts_with(name)) << message.chars();
    EXPECT_EQ(message.chars().substr(name.size()),
              environment::error_explanation(access_denied).chars());
    EXPECT_GT(message.chars().size(), name.size()) << message.chars();
}

// Any other error is its own text, and what of it is not UTF-8 comes out marked, not lost.
TEST(EnvironmentTest, ErrorMessageKeepsWhatIsNotUtf8Marked) {
    EXPECT_EQ(environment::error_message(std::runtime_error{"cannot read \xC2\xA0"
                                                            "file \xFF here"})
                  .chars(),
              "cannot read \xC2\xA0"
              "file \xEF\xBF\xBD here");
}
