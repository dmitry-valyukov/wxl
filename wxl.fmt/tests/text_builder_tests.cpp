#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>

#include <fmt/compile.h>
#include <fmt/format.h>

// wxl.fmt imports wxl.core without re-exporting it, so the allocator a buffer
// runs on is the caller's own import.
import wxl.core;
import wxl.fmt;

using namespace std::string_view_literals;

namespace {

/// A builder on the STA pool, which is what the whole allocator parameter is
/// there for.
using pooled_builder = wxl::core::text_builder<wxl::core::sta_allocator>;

TEST(builder, formatting_appends) {
    wxl::core::text_builder<> out;

    out.format("{}x{}", 3, 4);
    out.append(" -- "sv);
    out.format("{:.1f}%", 12.34);

    EXPECT_EQ(out.view(), "3x4 -- 12.3%");
    EXPECT_EQ(out.size(), 12u);
}

TEST(builder, appending_takes_text_as_it_stands) {
    wxl::core::text_builder<> out;

    // The brace is a brace here, which is why appending is not formatting.
    out.append("a {title} with braces"sv);
    out.append('!');

    EXPECT_EQ(out.view(), "a {title} with braces!");
}

TEST(builder, a_compiled_format_string) {
    wxl::core::text_builder<> out;

    out.format(FMT_COMPILE("{} of {}"), 7, 350);

    EXPECT_EQ(out.view(), "7 of 350");
}

TEST(builder, a_format_string_decided_at_run_time) {
    const std::string form = "{}-{}";

    wxl::core::text_builder<> out;
    out.format(fmt::runtime(form), "a", "b");

    EXPECT_EQ(out.view(), "a-b");
}

TEST(builder, an_empty_builder) {
    const wxl::core::text_builder<> out;

    EXPECT_TRUE(out.empty());
    EXPECT_EQ(out.view(), ""sv);
}

TEST(builder, growing_keeps_everything_already_written) {
    wxl::core::text_builder<> out;

    std::string expected;

    for (int number = 0; number != 2000; ++number) {
        out.format("{},", number);
        expected += std::to_string(number);
        expected += ',';
    }

    EXPECT_GT(out.size(), wxl::core::buffered_capacity);
    EXPECT_EQ(out.view(), expected);
}

TEST(builder, resetting_keeps_the_memory) {
    wxl::core::text_builder<> out;

    out.format("{}", std::string(1000, 'x'));

    const std::size_t held = out.buffer().capacity();

    ASSERT_GT(held, wxl::core::buffered_capacity);

    out.reset();

    EXPECT_TRUE(out.empty());
    EXPECT_EQ(out.buffer().capacity(), held) << "the memory was given back";

    out.format("{}", 1);
    EXPECT_EQ(out.view(), "1");
}

TEST(builder, a_short_line_never_allocates) {
    wxl::core::text_builder<> out;

    const char* const inside = out.buffer().data();

    out.format("page {} of {}", 7, 350);

    EXPECT_EQ(out.buffer().data(), inside) << "a line this short should fit inside the builder";
    EXPECT_EQ(out.buffer().capacity(), wxl::core::buffered_capacity);
}

TEST(builder, on_the_pool) {
    pooled_builder out;

    out.format(FMT_COMPILE("page {} of {}"), 7, 350);

    EXPECT_EQ(out.view(), "page 7 of 350");
}

TEST(builder, on_the_pool_it_grows_too) {
    pooled_builder out;

    out.format("{}", std::string(3000, 'z'));

    EXPECT_EQ(out.size(), 3000u);
    EXPECT_EQ(out.view(), std::string(3000, 'z'));
}

TEST(buffer, formatting_into_a_buffer_directly) {
    wxl::core::text_buffer<> room;

    fmt::format_to(fmt::appender(room), "{} {}", "into", "fmt");

    EXPECT_EQ(wxl::core::view_of(room), "into fmt");
}

TEST(buffer, a_buffer_carries_room_of_its_own) {
    wxl::core::text_buffer<> room;

    EXPECT_EQ(room.capacity(), wxl::core::buffered_capacity);

    room.reserve(1000);

    EXPECT_GE(room.capacity(), 1000u);
}

}  // namespace
