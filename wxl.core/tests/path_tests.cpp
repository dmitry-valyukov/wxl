#include <crtdbg.h>

#include <gtest/gtest.h>

import std;
import wxl.core;

using wxl::core::path;

namespace {

// The pool these paths allocate from is built once for the whole binary, by the
// global environment in sta_allocator_tests.cpp.

/// Longer than the small-string buffer, so the string really goes to the
/// allocator instead of living inside itself.
constexpr const wchar_t* long_name = L"a-name-far-longer-than-any-small-string-buffer.xml";

}  // namespace

TEST(PathTest, KeepsWhatItWasGivenAndIsNullTerminated) {
    const path p = L"C:\\data\\settings.xml";

    EXPECT_EQ(p.native(), std::wstring_view(L"C:\\data\\settings.xml"));
    EXPECT_EQ(std::wcslen(p.c_str()), p.size());
    EXPECT_FALSE(p.empty());
}

TEST(PathTest, DefaultConstructedIsEmpty) {
    const path p;

    EXPECT_TRUE(p.empty());
    EXPECT_EQ(p.size(), 0u);
    EXPECT_EQ(p.c_str()[0], L'\0');
}

TEST(PathTest, JoinsComponentsWithASeparator) {
    const path p = path(L"C:\\data") / L"files" / L"guid.xml";

    EXPECT_EQ(p.native(), std::wstring_view(L"C:\\data\\files\\guid.xml"));
}

TEST(PathTest, DoesNotDoubleASeparatorThatIsAlreadyThere) {
    EXPECT_EQ((path(L"C:\\data\\") / L"files").native(), std::wstring_view(L"C:\\data\\files"));
    EXPECT_EQ((path(L"C:/data/") / L"files").native(), std::wstring_view(L"C:/data/files"));
}

TEST(PathTest, JoiningOntoAnEmptyPathGivesTheComponent) {
    EXPECT_EQ((path() / L"files").native(), std::wstring_view(L"files"));
}

TEST(PathTest, AnEmptyComponentChangesNothing) {
    const path directory = L"C:\\data\\cache";

    EXPECT_EQ((directory / L"").native(), directory.native());
}

TEST(PathTest, FilenameIsTheLastComponent) {
    EXPECT_EQ(path(L"C:\\data\\file.dat").filename(), std::wstring_view(L"file.dat"));
    EXPECT_EQ(path(L"C:/data/file.dat").filename(), std::wstring_view(L"file.dat"));
    EXPECT_EQ(path(L"file.dat").filename(), std::wstring_view(L"file.dat"));
    EXPECT_EQ(path(L"C:\\data\\").filename(), std::wstring_view(L""));
}

TEST(PathTest, ParentPathKeepsARootSeparatorAndDropsAnyOther) {
    EXPECT_EQ(path(L"C:\\data\\file.dat").parent_path(), std::wstring_view(L"C:\\data"));
    EXPECT_EQ(path(L"C:\\file.dat").parent_path(), std::wstring_view(L"C:\\"));
    EXPECT_EQ(path(L"\\file.dat").parent_path(), std::wstring_view(L"\\"));
    EXPECT_EQ(path(L"file.dat").parent_path(), std::wstring_view(L""));
}

TEST(PathTest, ComparesOrdinally) {
    EXPECT_EQ(path(L"C:\\data\\file.dat"), path(L"C:\\data\\file.dat"));
    EXPECT_FALSE(path(L"C:\\data\\file.dat") == path(L"C:\\DATA\\FILE.DAT"));
}

// The debug CRT heap is what the probe below reads, and it is only there in a
// debug build.
#ifdef _DEBUG

TEST(PathTest, TakesItsCharactersFromTheStaPoolAndNotTheCrtHeap) {
    _CrtMemState before{}, after{}, difference{};

    _CrtMemCheckpoint(&before);
    path p = path(L"C:\\data\\files") / long_name;
    _CrtMemCheckpoint(&after);

    EXPECT_EQ(0, _CrtMemDifference(&difference, &before, &after))
        << "the characters came from the CRT heap rather than from sta_memory_pool";

    // Kept alive past the second checkpoint, or the probe would be comparing a
    // heap that had already been given everything back.
    EXPECT_EQ(p.filename(), std::wstring_view(long_name));
}

TEST(PathTest, TheProbeNoticesAnOrdinaryStdWstring) {
    _CrtMemState before{}, after{}, difference{};

    _CrtMemCheckpoint(&before);
    std::wstring text = std::wstring(L"C:\\data\\files\\") + long_name;
    _CrtMemCheckpoint(&after);

    EXPECT_NE(0, _CrtMemDifference(&difference, &before, &after));
    EXPECT_FALSE(text.empty());
}

#endif

TEST(PathTest, HandlesAUncPathTheWayWindowsWritesIt) {
    const path p = path(LR"(\\server\share)") / L"archive" / L"one.dat";

    EXPECT_EQ(p.native(), std::wstring_view(LR"(\\server\share\archive\one.dat)"));
    EXPECT_EQ(p.filename(), std::wstring_view(L"one.dat"));
    EXPECT_EQ(p.parent_path(), std::wstring_view(LR"(\\server\share\archive)"));
}
