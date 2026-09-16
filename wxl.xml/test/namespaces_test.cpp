#include <gtest/gtest.h>

#include <string_view>

import wxl.xml;

using namespace wxl::xml;

namespace {

constexpr std::string_view fb3_body = "http://www.fictionbook.org/FictionBook3/body";
constexpr std::string_view xlink = "http://www.w3.org/1999/xlink";

}  // namespace

/// What a name means is the namespace it stands in; the prefix a document
/// happens to spell it with is the document's own business. Both files below
/// say the same thing, and code that reads them says it once.
TEST(namespaces, the_same_document_written_with_and_without_a_prefix) {
    document plain;
    const node& a = plain.load(R"(<fb3-body xmlns=")" + std::string(fb3_body) + R"("><p/></fb3-body>)");

    EXPECT_EQ(a.name(), "fb3-body");
    EXPECT_EQ(a.prefix(), "");
    EXPECT_EQ(a.namespace_uri(), fb3_body);
    EXPECT_EQ(a.child("p")->namespace_uri(), fb3_body);

    document prefixed;
    const node& b =
        prefixed.load(R"(<fb3:fb3-body xmlns:fb3=")" + std::string(fb3_body) + R"("><fb3:p/></fb3:fb3-body>)");

    EXPECT_EQ(b.name(), "fb3-body");
    EXPECT_EQ(b.prefix(), "fb3");
    EXPECT_EQ(b.qualified_name(), "fb3:fb3-body");
    EXPECT_EQ(b.namespace_uri(), fb3_body);
    EXPECT_NE(b.child("p"), nullptr);
}

/// The default namespace covers elements only: an attribute without a prefix
/// belongs to the element carrying it, not to a vocabulary.
TEST(namespaces, an_unprefixed_attribute_is_in_no_namespace) {
    document doc;
    const node& root = doc.load(R"(<a xmlns="urn:x" href="here"/>)");

    EXPECT_EQ(root.namespace_uri(), "urn:x");
    ASSERT_EQ(root.attributes().size(), 2u);  // xmlns is an attribute too

    const attribute& href = root.attributes().back();
    EXPECT_EQ(href.name(), "href");
    EXPECT_EQ(href.namespace_uri(), "");
}

TEST(namespaces, a_prefixed_attribute_takes_its_prefixs_namespace) {
    document doc;
    const node& root =
        doc.load(R"(<a xmlns:xlink="http://www.w3.org/1999/xlink" xlink:href="target"/>)");

    EXPECT_EQ(root.attribute("href"), "target");                 // by local name
    EXPECT_EQ(root.attribute(xlink, "href"), "target");          // and by both
    EXPECT_EQ(root.attribute("urn:other", "href"), std::nullopt);
}

/// An inner declaration holds until the element it was written on ends.
TEST(namespaces, a_declaration_reaches_as_far_as_its_element) {
    document doc;
    const node& root = doc.load(R"(<a xmlns="urn:outer"><b xmlns="urn:inner"><c/></b><d/></a>)");

    EXPECT_EQ(root.namespace_uri(), "urn:outer");

    const node* const b = root.child("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->namespace_uri(), "urn:inner");
    EXPECT_EQ(b->child("c")->namespace_uri(), "urn:inner");

    // Back outside: what b declared went with it.
    EXPECT_EQ(root.child("d")->namespace_uri(), "urn:outer");
}

TEST(namespaces, an_empty_element_declares_only_for_itself) {
    document doc;
    const node& root = doc.load(R"(<a xmlns="urn:outer"><b xmlns="urn:inner"/><c/></a>)");

    EXPECT_EQ(root.child("b")->namespace_uri(), "urn:inner");
    EXPECT_EQ(root.child("c")->namespace_uri(), "urn:outer");
}

/// The two namespaces XML binds itself, which no document declares.
TEST(namespaces, xml_and_xmlns_are_bound_without_being_declared) {
    document doc;
    const node& root = doc.load(R"(<a xml:lang="ru" xmlns="urn:x"/>)");

    EXPECT_EQ(root.attribute("http://www.w3.org/XML/1998/namespace", "lang"), "ru");
    EXPECT_EQ(root.attributes().back().namespace_uri(), "http://www.w3.org/2000/xmlns/");
}

/// A prefix nothing declared is not a reason to refuse the file: the name
/// keeps what the document wrote and stands in no namespace.
TEST(namespaces, an_undeclared_prefix_leaves_the_name_where_it_was_written) {
    document doc;
    const node& root = doc.load(R"(<x:a y:attr="v"/>)");

    EXPECT_EQ(root.name(), "a");
    EXPECT_EQ(root.prefix(), "x");
    EXPECT_EQ(root.qualified_name(), "x:a");
    EXPECT_EQ(root.namespace_uri(), "");
    EXPECT_EQ(root.attribute("attr"), "v");
}

/// An end tag matches the start tag as written, prefix and all.
TEST(namespaces, an_end_tag_matches_the_prefix_too) {
    document doc;
    EXPECT_THROW(doc.load(R"(<x:a xmlns:x="urn:x"></a>)"), parsing_exception);
}
