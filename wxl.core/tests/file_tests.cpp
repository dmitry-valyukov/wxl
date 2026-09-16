#include <gtest/gtest.h>

import std;
import wxl.core;

using wxl::core::file;
using wxl::core::path;

namespace {

path temp_file(const wchar_t* name) {
    return path(std::filesystem::temp_directory_path().wstring()) / name;
}

std::span<const std::byte> bytes_of(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

/// Writes the file the reading tests then read, and asserts on the way that the
/// writing half worked -- a read that fails is otherwise indistinguishable from
/// a write that never happened.
void given_a_file(const path& p, std::string_view content) {
    file out = file::create(p.c_str());

    ASSERT_TRUE(out.opened());
    ASSERT_EQ(out.write(bytes_of(content)), content.size());
    ASSERT_TRUE(out.flush());
}

}  // namespace

TEST(FileTest, DefaultConstructedIsClosedAndDoesNothing) {
    file f;

    std::byte buffer[8];

    EXPECT_FALSE(f.opened());
    EXPECT_EQ(f.read(buffer), 0u);
    EXPECT_EQ(f.write(bytes_of("x")), 0u);
    EXPECT_FALSE(f.size().has_value());
    EXPECT_FALSE(f.flush());
}

TEST(FileTest, OpeningWhatIsNotThereLeavesItClosed) {
    const path p = temp_file(L"wxl_core_file_tests.no_such_file");

    std::filesystem::remove(p.native());

    file f = file::open_read(p.c_str());

    EXPECT_FALSE(f.opened());
}

TEST(FileTest, WritesAndReadsBackTheSameBytes) {
    const path p = temp_file(L"wxl_core_file_tests.bin");
    const std::string content = "wxl::core::file, a hundred and one bytes or so of it, twice over.";

    given_a_file(p, content);

    file in = file::open_read(p.c_str());

    ASSERT_TRUE(in.opened());
    ASSERT_TRUE(in.size().has_value());
    EXPECT_EQ(*in.size(), content.size());

    std::string got(content.size(), '\0');

    EXPECT_EQ(in.read({reinterpret_cast<std::byte*>(got.data()), got.size()}), content.size());
    EXPECT_EQ(got, content);

    // Past the end: nothing more to give, and no error either.
    std::byte tail[8];
    EXPECT_EQ(in.read(tail), 0u);

    in.close();
    std::filesystem::remove(p.native());
}

TEST(FileTest, ReadsAsMuchAsIsThereWhenAskedForMore) {
    const path p = temp_file(L"wxl_core_file_tests.short");
    const std::string content = "seven!!";

    given_a_file(p, content);

    file in = file::open_read(p.c_str());

    ASSERT_TRUE(in.opened());

    std::byte buffer[1024];

    EXPECT_EQ(in.read(buffer), content.size());

    in.close();
    std::filesystem::remove(p.native());
}

TEST(FileTest, CreatingOverAnExistingFileEmptiesIt) {
    const path p = temp_file(L"wxl_core_file_tests.truncated");

    given_a_file(p, "the first thing that was written here");
    given_a_file(p, "shorter");

    file in = file::open_read(p.c_str());

    ASSERT_TRUE(in.opened());
    ASSERT_TRUE(in.size().has_value());
    EXPECT_EQ(*in.size(), 7u);

    in.close();
    std::filesystem::remove(p.native());
}

TEST(FileTest, MovingTakesTheHandleAndLeavesTheOtherClosed) {
    const path p = temp_file(L"wxl_core_file_tests.moved");

    given_a_file(p, "moved");

    file first = file::open_read(p.c_str());

    ASSERT_TRUE(first.opened());

    file second = std::move(first);

    EXPECT_TRUE(second.opened());
    EXPECT_FALSE(first.opened());

    std::byte buffer[16];

    EXPECT_EQ(second.read(buffer), 5u);

    second.close();
    EXPECT_FALSE(second.opened());

    // Closing twice is not an error: close() is also what the destructor calls.
    second.close();

    std::filesystem::remove(p.native());
}
