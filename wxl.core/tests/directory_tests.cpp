#include <gtest/gtest.h>

import std;
import wxl.core;

using wxl::core::directory;
using wxl::core::file;
using wxl::core::path;

namespace {

/// A directory of its own for every test, removed however the test ends.
///
/// Built with the class under test where the class can do it, and taken apart
/// with `std::filesystem`: a fixture that leans on what it is testing to clean
/// up leaves rubbish behind exactly when the test found a bug.
class DirectoryTest : public ::testing::Test
{
protected:
    void SetUp() override {
        root_ = path(std::filesystem::temp_directory_path().wstring()) / L"wxl_core_directory_tests";

        std::filesystem::remove_all(root_.native());

        ASSERT_TRUE(directory::create(root_.c_str()));
    }

    void TearDown() override { std::filesystem::remove_all(root_.native()); }

    void given_a_file(const wchar_t* name, std::string_view content = "") {
        file out = file::create((root_ / name).c_str());

        ASSERT_TRUE(out.opened());
        ASSERT_EQ(out.write({reinterpret_cast<const std::byte*>(content.data()), content.size()}),
                  content.size());
    }

    void given_a_directory(const wchar_t* name) {
        ASSERT_TRUE(directory::create((root_ / name).c_str()));
    }

    /// Everything the listing hands over, sorted, so that a test can compare it
    /// without depending on the order the file system happens to keep.
    std::vector<std::wstring> names_in(const wchar_t* mask = L"*") {
        directory listing = directory::open((root_ / mask).c_str());

        std::vector<std::wstring> found;
        directory::entry entry;

        while (listing.next(entry)) found.emplace_back(entry.name);

        std::ranges::sort(found);

        return found;
    }

    path root_;
};

}  // namespace

TEST_F(DirectoryTest, DefaultConstructedIsClosedAndHasNothingToGive) {
    directory listing;
    directory::entry entry;

    EXPECT_FALSE(listing.opened());
    EXPECT_FALSE(listing.next(entry));
}

TEST_F(DirectoryTest, OpeningWhatIsNotThereLeavesItClosed) {
    directory listing = directory::open((root_ / L"no_such_directory" / L"*").c_str());

    EXPECT_FALSE(listing.opened());
}

TEST_F(DirectoryTest, AnEmptyDirectoryListsNothingAtAll) {
    // Not even "." and "..", which is the point: the listing is open and
    // working, and still has nothing to say.
    directory listing = directory::open((root_ / L"*").c_str());

    directory::entry entry;

    EXPECT_TRUE(listing.opened());
    EXPECT_FALSE(listing.next(entry));
}

TEST_F(DirectoryTest, ListsFilesAndDirectories) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_directory(L"nested");

    EXPECT_EQ(names_in(), (std::vector<std::wstring>{L"nested", L"one.fb3", L"two.fb3"}));
}

TEST_F(DirectoryTest, TellsADirectoryFromAFileAndKnowsItsSize) {
    given_a_file(L"book.fb3", "0123456789");
    given_a_directory(L"nested");

    directory listing = directory::open((root_ / L"*").c_str());

    directory::entry entry;
    bool seen_file = false;
    bool seen_directory = false;

    while (listing.next(entry)) {
        if (entry.name == L"book.fb3") {
            seen_file = true;
            EXPECT_FALSE(entry.is_directory);
            EXPECT_EQ(entry.size, 10u);
        } else if (entry.name == L"nested") {
            seen_directory = true;
            EXPECT_TRUE(entry.is_directory);
        }
    }

    EXPECT_TRUE(seen_file);
    EXPECT_TRUE(seen_directory);
}

TEST_F(DirectoryTest, TheMaskIsTheSystemsToApply) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_file(L"notes.txt");

    EXPECT_EQ(names_in(L"*.fb3"), (std::vector<std::wstring>{L"one.fb3", L"two.fb3"}));
}

TEST_F(DirectoryTest, ANameIsGoodUntilTheNextStep) {
    given_a_file(L"first.fb3");
    given_a_file(L"second.fb3");

    directory listing = directory::open((root_ / L"*").c_str());

    directory::entry entry;

    ASSERT_TRUE(listing.next(entry));

    const std::wstring kept(entry.name);

    ASSERT_TRUE(listing.next(entry));

    // The buffer was reused, exactly as documented -- which is why a caller that
    // wants to keep a name copies it, as this test just did.
    EXPECT_NE(entry.name, kept);
}

TEST_F(DirectoryTest, ClosingEndsTheListing) {
    given_a_file(L"one.fb3");

    directory listing = directory::open((root_ / L"*").c_str());

    ASSERT_TRUE(listing.opened());

    listing.close();

    directory::entry entry;

    EXPECT_FALSE(listing.opened());
    EXPECT_FALSE(listing.next(entry));

    // Closing twice is what the destructor does after an explicit close.
    listing.close();
}

TEST_F(DirectoryTest, MovingTakesTheListingAndLeavesTheOtherClosed) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");

    directory first = directory::open((root_ / L"*").c_str());

    ASSERT_TRUE(first.opened());

    directory second = std::move(first);

    EXPECT_TRUE(second.opened());
    EXPECT_FALSE(first.opened());

    directory::entry entry;
    int count = 0;

    while (second.next(entry)) ++count;

    EXPECT_EQ(count, 2);
}

TEST_F(DirectoryTest, ExistsAnswersForDirectoriesOnly) {
    given_a_file(L"book.fb3");
    given_a_directory(L"nested");

    EXPECT_TRUE(directory::exists(root_.c_str()));
    EXPECT_TRUE(directory::exists((root_ / L"nested").c_str()));
    EXPECT_FALSE(directory::exists((root_ / L"book.fb3").c_str()));
    EXPECT_FALSE(directory::exists((root_ / L"no_such_thing").c_str()));
}

TEST_F(DirectoryTest, CreateMakesOneLevelAndIsHappyWithOneThatIsThere) {
    const path nested = root_ / L"nested";

    EXPECT_TRUE(directory::create(nested.c_str()));
    EXPECT_TRUE(directory::exists(nested.c_str()));

    // Already there is also "there".
    EXPECT_TRUE(directory::create(nested.c_str()));

    // A parent that is not there is not made on the way.
    EXPECT_FALSE(directory::create((root_ / L"missing" / L"child").c_str()));
}

TEST_F(DirectoryTest, CreateAllMakesTheWholeChainAndLeavesThePathAsItWas) {
    path deep = root_ / L"one" / L"two" / L"three";

    const std::wstring before(deep.native());

    EXPECT_TRUE(directory::create_all(deep));

    EXPECT_EQ(deep.native(), before);
    EXPECT_EQ(std::wcslen(deep.c_str()), deep.size());

    EXPECT_TRUE(directory::exists((root_ / L"one").c_str()));
    EXPECT_TRUE(directory::exists((root_ / L"one" / L"two").c_str()));
    EXPECT_TRUE(directory::exists(deep.c_str()));
}

TEST_F(DirectoryTest, CreateAllIsHappyWithAChainThatIsAlreadyThere) {
    path deep = root_ / L"one" / L"two";

    ASSERT_TRUE(directory::create_all(deep));
    EXPECT_TRUE(directory::create_all(deep));
}

TEST_F(DirectoryTest, CreateAllOnAnEmptyPathFails) {
    path nothing;

    EXPECT_FALSE(directory::create_all(nothing));
}

TEST_F(DirectoryTest, RemoveTakesAnEmptyDirectoryAndLeavesAFullOne) {
    const path nested = root_ / L"nested";

    ASSERT_TRUE(directory::create(nested.c_str()));

    EXPECT_TRUE(directory::remove(nested.c_str()));
    EXPECT_FALSE(directory::exists(nested.c_str()));

    ASSERT_TRUE(directory::create(nested.c_str()));
    file out = file::create((nested / L"book.fb3").c_str());
    ASSERT_TRUE(out.opened());
    out.close();

    EXPECT_FALSE(directory::remove(nested.c_str()));
    EXPECT_TRUE(directory::exists(nested.c_str()));
}

TEST_F(DirectoryTest, RemovingWhatIsNotThereFails) {
    EXPECT_FALSE(directory::remove((root_ / L"no_such_thing").c_str()));
}

TEST_F(DirectoryTest, CreateAllPutsUpWithATrailingSeparator) {
    const path deep = root_ / L"one" / L"two";

    // A path written the way a person writes a directory -- with the separator
    // still on the end.
    path with_separator = path(std::wstring(deep.native()) + L"\\");

    EXPECT_TRUE(directory::create_all(with_separator));
    EXPECT_TRUE(directory::exists(deep.c_str()));
}

TEST_F(DirectoryTest, CreateAllLeavesTheRootAlone) {
    // A path whose every level below the root is already there: nothing to make,
    // and above all no attempt to make "C:" or a UNC share, which would fail and
    // sink the whole call.
    path existing = root_;

    EXPECT_TRUE(directory::create_all(existing));
}
