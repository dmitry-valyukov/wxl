#include <gtest/gtest.h>

#include <crtdbg.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

// ---- What goes in, and what does not -----------------------------------------------
//
// The zero after the text is either promised by the type or written out at the
// one door that trusts its caller. A plain view has no promise to make, so it
// does not convert -- and neither does anything that only wraps one.

static_assert(std::is_convertible_v<const char16_t*, zstring_view>);
static_assert(std::is_convertible_v<const char16_t (&)[4], zstring_view>);
static_assert(std::is_convertible_v<const std::u16string&, zstring_view>);
static_assert(std::is_convertible_v<
              const std::basic_string<char16_t, std::char_traits<char16_t>, sta_allocator<char16_t>>&,
              zstring_view>);
static_assert(std::is_convertible_v<const u16_text&, zstring_view>);
static_assert(std::is_convertible_v<const hstring&, zstring_view>);

// The seam of wchar_t, which is still what half of the tree writes.
static_assert(std::is_convertible_v<const wchar_t*, zstring_view>);
static_assert(std::is_convertible_v<const std::wstring&, zstring_view>);

static_assert(!std::is_convertible_v<std::u16string_view, zstring_view>);
static_assert(!std::is_convertible_v<std::wstring_view, zstring_view>);
static_assert(!std::is_convertible_v<u16_view, zstring_view>);
static_assert(!std::is_constructible_v<zstring_view, std::u16string_view>);

// Out of it, a view is one conversion away, and nothing else is promised.
static_assert(std::is_convertible_v<zstring_view, std::u16string_view>);
static_assert(!std::is_convertible_v<std::u16string_view, zstring_view>);

TEST(zstring_view, empty_is_terminated_too) {
    constexpr zstring_view empty;

    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.data()[0], u'\0');
}

TEST(zstring_view, a_c_string_measures_up_to_its_zero) {
    const zstring_view text = u"Привет";

    EXPECT_EQ(text.size(), 6u);
    EXPECT_EQ(text.data()[text.size()], u'\0');
    EXPECT_TRUE(text == u"Привет");
    EXPECT_FALSE(text == u"Привет!");
}

TEST(zstring_view, a_string_keeps_its_own_zero) {
    const std::u16string owner = u"ёж";
    const zstring_view text = owner;

    EXPECT_EQ(text.data(), owner.c_str());
    EXPECT_EQ(text.size(), 2u);
}

TEST(zstring_view, owned_checked_text_is_terminated_for_nothing) {
    const u16_text owner{u"ёж😀"};
    const zstring_view text = owner;

    EXPECT_EQ(text.data(), owner.plain().c_str());
    EXPECT_EQ(text.size(), owner.size());
    EXPECT_EQ(text.data()[text.size()], u'\0');
}

TEST(zstring_view, an_hstring_is_terminated_for_nothing) {
    const hstring owner{u"Привет"};
    const zstring_view text = owner;

    EXPECT_EQ(text.data(), owner.data());
    EXPECT_EQ(text.size(), 6u);
    EXPECT_EQ(text.data()[text.size()], u'\0');
}

TEST(zstring_view, the_seam_of_wchar_t_reads_the_same_units) {
    const zstring_view text = L"Привет";

    EXPECT_EQ(text.size(), 6u);
    EXPECT_TRUE(text == u"Привет");
    EXPECT_EQ(std::wstring_view(text.wc_str(), text.size()), L"Привет");
    EXPECT_EQ(text.wide(), L"Привет");
}

TEST(zstring_view, a_slice_to_the_end_keeps_the_zero_and_a_counted_one_does_not) {
    const zstring_view text = u"Привет";

    const zstring_view tail = text.substr(2);
    EXPECT_TRUE(tail == u"ивет");
    EXPECT_EQ(tail.data()[tail.size()], u'\0');

    const std::u16string_view middle = text.substr(2, 2);
    EXPECT_TRUE(middle == u"ив");
}

TEST(zstring_view, cutting_from_the_front_keeps_the_zero) {
    zstring_view text = u"Привет";
    text.remove_prefix(3);

    EXPECT_TRUE(text == u"вет");
    EXPECT_EQ(text.data()[text.size()], u'\0');
}

TEST(zstring_view, compares_with_views_and_literals_without_asking_which) {
    const zstring_view text = u"abc";
    const std::u16string_view view = u"abc";

    EXPECT_TRUE(text == view);
    EXPECT_TRUE(text == u"abc");
    EXPECT_TRUE(text == zstring_view{u"abc"});
    EXPECT_TRUE(text < zstring_view{u"abd"});
}

// ---- The one door that trusts ----------------------------------------------------------

TEST(zstring_view, a_view_that_is_terminated_passes_the_door) {
    // The arena's shape: the string, and one unit more.
    const char16_t arena[] = {u'a', u'b', u'c', u'\0'};
    const auto text = assume_terminated(std::u16string_view(arena, 3));

    EXPECT_EQ(text.size(), 3u);
    EXPECT_EQ(text.data(), arena);
}

TEST(zstring_view, an_empty_view_over_nothing_passes_the_door) {
    const auto text = assume_terminated(std::u16string_view{});

    EXPECT_TRUE(text.empty());
    EXPECT_EQ(text.data()[0], u'\0');
}

TEST(zstring_view, the_seam_door_takes_a_wstring_view) {
    const wchar_t arena[] = {L'a', L'b', L'\0'};
    const auto text = assume_terminated(std::wstring_view(arena, 2));

    EXPECT_EQ(text.size(), 2u);
    EXPECT_TRUE(text == u"ab");
}

// The promise broken: the unit after the view is not a zero. It is one unit
// inside a buffer here, so the read is not itself a fault. It takes the process
// down in every build, and to stderr rather than to a dialog that would hang the
// run.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void trust_a_view_with_no_zero_after_it() {
    report_failures_to_stderr();

    const char16_t buffer[] = {u'a', u'b', u'c', u'x'};
    (void)assume_terminated(std::u16string_view(buffer, 3));
}

TEST(zstring_view_death_tests, a_view_with_no_zero_after_it_takes_the_process_down) {
    EXPECT_DEATH(trust_a_view_with_no_zero_after_it(), "");
}

}  // namespace
