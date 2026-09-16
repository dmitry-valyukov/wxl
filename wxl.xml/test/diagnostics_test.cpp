#include <gtest/gtest.h>

#include <istream>
#include <sstream>
#include <streambuf>
#include <string>

import wxl.xml;

using namespace wxl::xml;

namespace {

/// Where a document was refused, as a pair, so a test reads as the place it
/// expects rather than as two assertions about one failure.
std::pair<int, int> refused_at(document& p, std::string source) {
    try {
        p.load(std::move(source));
    } catch (const parsing_exception& e) {
        return {e.line(), e.column()};
    }

    return {0, 0};
}

/// A stream that hands over what it holds and then breaks, the way a file on a
/// failing disk does. Throwing out of underflow is what sets badbit.
class failing_buf : public std::streambuf {
public:
    explicit failing_buf(std::string text) : text_(std::move(text)) {}

protected:
    int_type underflow() override {
        if (served_)
            throw std::ios_base::failure("the device is gone");

        served_ = true;
        setg(text_.data(), text_.data(), text_.data() + text_.size());

        return text_.empty() ? traits_type::eof() : traits_type::to_int_type(*gptr());
    }

private:
    std::string text_;
    bool served_ = false;
};

}  // namespace

// A rule that refuses something it had already begun reports the beginning:
// the byte it gave up on says only that the document ran out, and what was
// left open may be any distance back.
TEST(xml_diagnostics, what_was_left_open_is_what_is_reported) {
    document p;

    // The node never closed -- not the end tag disagreeing with it, and not
    // the end of the document.
    EXPECT_EQ(refused_at(p, "<a>\n  <b>\n</a>"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "<a>\n  <b>\n"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "<a>\n  <b>\n    <c>\n"), std::pair(3, 5));

    EXPECT_EQ(refused_at(p, "<a>\n  <!-- on and on\n</a>"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "<a>\n  <![CDATA[ text\n</a>"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "\n<?target data\n<a/>"), std::pair(2, 1));
    EXPECT_EQ(refused_at(p, "<a\n  x='1\n"), std::pair(2, 5));

    // The internal subset, at the bracket that opened it -- the declaration
    // around it is not what is unfinished.
    EXPECT_EQ(refused_at(p, "\n<!DOCTYPE a [<!ENTITY x 'y'>\n<a/>"), std::pair(2, 13));

    // Nothing may follow an unterminated declaration: a '>' anywhere behind it
    // would close it, markup or not, since it is read and not understood.
    EXPECT_EQ(refused_at(p, "\n<!DOCTYPE a SYSTEM 'x.dtd'\n"), std::pair(2, 1));
}

// Where the byte itself is the mistake, that byte is what is reported.
TEST(xml_diagnostics, what_is_wrong_where_it_stands_is_reported_there) {
    document p;

    EXPECT_EQ(refused_at(p, "<a x='1'y='2'/>"), std::pair(1, 9));
    EXPECT_EQ(refused_at(p, "<a x='1' x='2'/>"), std::pair(1, 10));
    EXPECT_EQ(refused_at(p, "<a/> text"), std::pair(1, 6));
    EXPECT_EQ(refused_at(p, std::string("<a>\x01</a>", 9)), std::pair(1, 4));
    EXPECT_EQ(refused_at(p, "<a x='<'/>"), std::pair(1, 7));
}

// An attribute is one construct: what is wrong with it is reported at its
// name, not at the byte where the reading of it stopped.
TEST(xml_diagnostics, a_broken_attribute_is_reported_at_its_name) {
    document p;

    EXPECT_EQ(refused_at(p, "<a\n  broken\n/>"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "<a\n  broken=unquoted/>"), std::pair(2, 3));
}

TEST(xml_diagnostics, a_broken_reference_is_reported_at_its_opening) {
    document p;

    EXPECT_EQ(refused_at(p, "<a>\n  &nosuch;\n</a>"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "<a>\n  &#xD800;\n</a>"), std::pair(2, 3));
    EXPECT_EQ(refused_at(p, "<a>\n  &#zz;\n</a>"), std::pair(2, 3));
}

TEST(xml_stream, a_stream_is_read_to_its_end) {
    std::istringstream source("<a x='1'><b/></a>");

    document p;
    const node& root = p.load(source, "from-a-stream.xml");

    EXPECT_EQ(root.name(), "a");
    EXPECT_EQ(root.required_attribute("x"), "1");
    EXPECT_EQ(p.file_name(), "from-a-stream.xml");
}

// A stream that breaks part way gives a document with its tail missing, which
// would parse as a broken one. Saying that the stream failed is what keeps its
// reader from looking for a mistake that is not there.
TEST(xml_stream, a_stream_that_fails_is_not_a_broken_document) {
    failing_buf buf("<a><b/>");
    std::istream source(&buf);

    document p;

    try {
        p.load(source);
        FAIL() << "a stream that fails has to throw";
    } catch (const parsing_exception&) {
        FAIL() << "the document is not what is broken here";
    } catch (const exception& e) {
        // Nothing, rather than the seven bytes the buffer had handed over: a
        // stream whose buffer throws reports no count for that read at all, so
        // the message carries what was confirmed.
        EXPECT_STREQ(e.what(), "the stream failed after 0 bytes");
    }
}

TEST(xml_stream, a_stream_that_fails_says_how_far_it_got) {
    constexpr std::size_t chunk = 64 * 1024;                // what the reader asks of a stream at a time

    failing_buf buf(std::string(chunk, ' ') + "<a/>");
    std::istream source(&buf);

    document p;

    try {
        p.load(source);
        FAIL() << "a stream that fails has to throw";
    } catch (const exception& e) {
        EXPECT_STREQ(e.what(), "the stream failed after 65536 bytes");
    }
}

TEST(xml_stream, a_stream_that_cannot_be_read_at_all) {
    std::istringstream source("<a/>");
    source.setstate(std::ios::failbit);

    document p;

    EXPECT_THROW(p.load(source), exception);
}
