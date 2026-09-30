// The layout is checked against the one that matters: the operating system's own
// functions read what hstring writes and write what hstring reads. If the header,
// the count or the heap were not the ones every WinRT component shares, this is
// where it would show.
#include "platform.h"

#include <gtest/gtest.h>

#include <winstring.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

HSTRING abi(const hstring& value) { return static_cast<HSTRING>(value.get_abi()); }

std::wstring_view raw_text(HSTRING handle) {
    UINT32 length = 0;
    const wchar_t* buffer = ::WindowsGetStringRawBuffer(handle, &length);
    return {buffer, length};
}

// ---- What is taken, and what is not --------------------------------------------------

static_assert(sizeof(hstring) == sizeof(void*));
static_assert(std::is_standard_layout_v<hstring>);

// A borrowed reference is built in place and stays there: its handle points into
// itself.
static_assert(!std::is_copy_constructible_v<hstring_param>);
static_assert(!std::is_move_constructible_v<hstring_param>);

// One step for anything that is a zstring_view on its own, including a literal
// and owned text; one for an hstring; nothing for a plain view.
static_assert(std::is_convertible_v<const hstring&, hstring_param>);
static_assert(std::is_convertible_v<zstring_view, hstring_param>);
static_assert(std::is_convertible_v<const char16_t (&)[4], hstring_param>);
static_assert(std::is_convertible_v<const char16_t*, hstring_param>);
static_assert(std::is_convertible_v<const std::u16string&, hstring_param>);
static_assert(std::is_convertible_v<const u16_text&, hstring_param>);
static_assert(std::is_convertible_v<const wchar_t (&)[4], hstring_param>);
static_assert(std::is_convertible_v<const std::wstring&, hstring_param>);

static_assert(!std::is_convertible_v<std::u16string_view, hstring_param>);
static_assert(!std::is_convertible_v<std::wstring_view, hstring_param>);
static_assert(!std::is_convertible_v<u16_view, hstring_param>);


// A parameter, as a function takes it: no overload to choose, however the text comes.
std::size_t length_of(hstring_param text) { return text.size(); }

TEST(hstring, default_is_empty_and_terminated) {
    const hstring empty;

    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.data()[0], u'\0');
    EXPECT_EQ(empty.get_abi(), nullptr);
}

TEST(hstring, holds_a_copy_of_the_text) {
    std::u16string source = u"Привет";
    const hstring text{source};
    source = u"XXXXXX";

    EXPECT_EQ(text.size(), 6u);
    EXPECT_TRUE(text == u"Привет");
    EXPECT_EQ(text.data()[text.size()], u'\0');
}

TEST(hstring, made_from_nothing_is_the_empty_handle) {
    const hstring text{std::u16string_view{}};

    EXPECT_EQ(text.get_abi(), nullptr);
}

TEST(hstring, made_from_checked_text) {
    const u16_text owner{u"ёж😀"};

    EXPECT_TRUE(hstring{owner} == u"ёж😀");
    EXPECT_TRUE(hstring{static_cast<u16_view>(owner)} == u"ёж😀");
}

TEST(hstring, a_copy_shares_the_text_and_the_count_holds_it) {
    hstring first{u"Привет"};
    const void* handle = first.get_abi();

    {
        const hstring second = first;
        EXPECT_EQ(second.get_abi(), handle);
        EXPECT_EQ(second.data(), first.data());
    }

    first = hstring{u"other"};
    EXPECT_NE(first.get_abi(), handle);
}

TEST(hstring, a_move_takes_the_reference_and_leaves_nothing) {
    hstring first{u"Привет"};
    const void* handle = first.get_abi();

    const hstring second = std::move(first);

    EXPECT_EQ(second.get_abi(), handle);
    EXPECT_EQ(first.get_abi(), nullptr);
}

TEST(hstring, comparing) {
    const hstring text{u"abc"};

    EXPECT_TRUE(text == hstring{u"abc"});
    EXPECT_TRUE(text == u"abc");
    EXPECT_TRUE(text == std::u16string_view{u"abc"});
    EXPECT_FALSE(text == u"abd");
}

// ---- The operating system reads what we write ------------------------------------------

TEST(hstring_and_the_system, reads_our_string) {
    const hstring text{u"Привет"};

    EXPECT_EQ(raw_text(abi(text)), L"Привет");
    EXPECT_FALSE(::WindowsIsStringEmpty(abi(text)));
    EXPECT_EQ(::WindowsGetStringLen(abi(text)), 6u);
}

TEST(hstring_and_the_system, sees_the_empty_string_as_empty) {
    const hstring text;

    EXPECT_TRUE(::WindowsIsStringEmpty(abi(text)));
}

TEST(hstring_and_the_system, a_duplicate_it_makes_is_our_count_going_up) {
    hstring text{u"Привет"};
    const HSTRING handle = abi(text);

    HSTRING duplicate = nullptr;
    ASSERT_EQ(::WindowsDuplicateString(handle, &duplicate), S_OK);
    EXPECT_EQ(duplicate, handle);

    // Ours goes; the system's reference keeps the text alive, and when it drops
    // the last one, it is the system that frees what we allocated.
    text = hstring{};
    EXPECT_EQ(raw_text(duplicate), L"Привет");
    EXPECT_EQ(::WindowsDeleteString(duplicate), S_OK);
}

TEST(hstring_and_the_system, a_fast_pass_string_of_ours_is_copied_for_real_by_a_duplicate) {
    const std::u16string owner = u"Привет";
    const hstring_param param{owner};
    const HSTRING reference = static_cast<HSTRING>(param.get_abi());

    HSTRING copy = nullptr;
    ASSERT_EQ(::WindowsDuplicateString(reference, &copy), S_OK);

    // Keeping the string means owning one: the system made a string of its own,
    // and it survives the characters it was made over.
    EXPECT_NE(copy, reference);
    EXPECT_EQ(raw_text(copy), L"Привет");
    EXPECT_NE(raw_text(copy).data(), reinterpret_cast<const wchar_t*>(owner.c_str()));
    EXPECT_EQ(::WindowsDeleteString(copy), S_OK);
}

TEST(hstring_and_the_system, an_hstring_lent_as_a_param_is_duplicated_by_count) {
    const hstring text{u"Привет"};
    const hstring_param param{text};

    HSTRING copy = nullptr;
    ASSERT_EQ(::WindowsDuplicateString(static_cast<HSTRING>(param.get_abi()), &copy), S_OK);
    EXPECT_EQ(copy, abi(text));
    EXPECT_EQ(::WindowsDeleteString(copy), S_OK);
}

// ---- We read what the operating system writes ---------------------------------------------

TEST(hstring_and_the_system, reads_its_string_and_frees_it_with_our_release) {
    HSTRING made = nullptr;
    ASSERT_EQ(::WindowsCreateString(L"Привет", 6, &made), S_OK);

    {
        hstring text = hstring::attach(made);
        EXPECT_TRUE(text == u"Привет");
        EXPECT_EQ(text.size(), 6u);
        EXPECT_EQ(text.data()[text.size()], u'\0');

        const hstring copy = text;
        EXPECT_EQ(copy.get_abi(), text.get_abi());
    }
    // Both references are ours and both are gone: the last decrement freed what
    // the system allocated, with HeapFree, and nothing crashed.
}

TEST(hstring_and_the_system, a_string_made_by_the_system_can_be_duplicated_by_us_and_dropped_by_it) {
    HSTRING made = nullptr;
    ASSERT_EQ(::WindowsCreateString(L"Привет", 6, &made), S_OK);

    hstring text = hstring::attach(made);
    HSTRING second = nullptr;
    ASSERT_EQ(::WindowsDuplicateString(made, &second), S_OK);

    text = hstring{};
    EXPECT_EQ(raw_text(second), L"Привет");
    EXPECT_EQ(::WindowsDeleteString(second), S_OK);
}

TEST(hstring_and_the_system, detach_hands_the_reference_over) {
    hstring text{u"Привет"};
    HSTRING handle = static_cast<HSTRING>(text.detach());

    EXPECT_EQ(text.get_abi(), nullptr);
    EXPECT_EQ(raw_text(handle), L"Привет");
    EXPECT_EQ(::WindowsDeleteString(handle), S_OK);
}

TEST(hstring_and_the_system, put_abi_takes_an_out_parameter) {
    hstring text{u"before"};

    HSTRING made = nullptr;
    ASSERT_EQ(::WindowsCreateString(L"after", 5, &made), S_OK);
    *reinterpret_cast<HSTRING*>(text.put_abi()) = made;

    EXPECT_TRUE(text == u"after");
}

// ---- hstring_param -------------------------------------------------------------------------

TEST(hstring_param, from_an_hstring_lends_the_handle) {
    const hstring text{u"Привет"};
    const hstring_param param{text};

    EXPECT_EQ(param.get_abi(), text.get_abi());
    EXPECT_EQ(param.size(), 6u);
}

TEST(hstring_param, from_text_is_a_fast_pass_string_over_the_same_characters) {
    const std::u16string owner = u"Привет";
    const hstring_param param{owner};

    // The characters are the owner's own, not a copy...
    EXPECT_EQ(param.text().data(), owner.c_str());
    EXPECT_EQ(param.size(), 6u);

    // ...and the system reads the header we wrote on the stack as one of its own.
    EXPECT_EQ(raw_text(static_cast<HSTRING>(param.get_abi())), L"Привет");
    EXPECT_EQ(::WindowsGetStringLen(static_cast<HSTRING>(param.get_abi())), 6u);
}

TEST(hstring_param, nothing_and_empty_are_the_empty_string) {
    const hstring_param none;
    const hstring_param empty{u""};

    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.get_abi(), nullptr);
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.get_abi(), nullptr);
    EXPECT_EQ(empty.text().data()[0], u'\0');
}

TEST(hstring_param, takes_however_the_text_comes) {
    const std::u16string owner = u"abc";
    const u16_text checked{u"abcd"};
    const hstring shared{u"abcde"};
    const wchar_t* wide = L"abcdef";

    EXPECT_EQ(length_of(u"ab"), 2u);
    EXPECT_EQ(length_of(owner), 3u);
    EXPECT_EQ(length_of(checked), 4u);
    EXPECT_EQ(length_of(shared), 5u);
    EXPECT_EQ(length_of(wide), 6u);
    EXPECT_EQ(length_of(zstring_view{u"abcdefg"}), 7u);
    EXPECT_EQ(length_of({}), 0u);
}

}  // namespace
