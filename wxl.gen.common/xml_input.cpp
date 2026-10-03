#include "xml_input.h"

import wxl.xml;

std::vector<std::pair<std::string, std::string>> nuspec_dependencies(
    std::filesystem::path const& nuspec) {
    std::vector<std::pair<std::string, std::string>> found;

    auto const collect = [&found](wxl::xml::node const& node) {
        auto const id = node.attribute("id");
        auto const version = node.attribute("version");
        if (id && version) {
            found.emplace_back(std::string{id->chars()}, std::string{version->chars()});
        }
    };

    wxl::xml::document document;
    auto const& root = document.load_file(nuspec);
    auto const* dependencies = root.find("dependencies");
    if (!dependencies) {
        return found;
    }

    // Dependencies are listed either directly or split into <group> elements
    // by target framework. The umbrella package uses the flat form, but the
    // grouped one is what nuspec generally allows, and either way every
    // <dependency> under here names the same package set.
    for (auto&& node : dependencies->children()) {
        if (node.name() == "group") {
            for (auto&& grouped : node.children()) {
                collect(grouped);
            }
        } else {
            collect(node);
        }
    }
    return found;
}

std::vector<dictionary_resource> dictionary_resources(std::filesystem::path const& dictionary) {
    std::vector<dictionary_resource> found;

    // Markup, not prose: an element here holds children or one piece of text,
    // never both, so the joining mode saves a node per run of text.
    wxl::xml::document document{wxl::xml::options{.keep_text = false}};

    auto const text = [](wxl::xml::node const& node, std::string_view name) {
        auto const value = node.attribute(name);
        return value ? std::string{value->chars()} : std::string{};
    };

    // The walk is an explicit stack rather than a function calling itself: a
    // resource dictionary nests as deep as the control templates in it, and
    // the reader is careful to make its own traversal iterative for exactly
    // that reason. The theme travels with the frame: it is a property of
    // where the walk stands, and a stack has no other memory of the path.
    struct frame {
        wxl::xml::node const* node;
        std::string theme;
        bool scoped;
    };
    std::vector<frame> pending{{&document.load_file(dictionary), {}, false}};
    while (!pending.empty()) {
        auto const [node, theme, scoped] = std::move(pending.back());
        pending.pop_back();

        // A key is what makes an element a resource; anything keyless is a
        // value inside somebody's setter. Local names throughout: the reader
        // resolves namespaces, so the prefix a dictionary happens to bind --
        // `x:Double`, `media:SolidColorBrush` -- is its business, and the
        // spellings merge on the name that matters.
        if (auto const key = node->attribute("Key")) {
            found.push_back({std::string{key->chars()}, std::string{node->name().chars()},
                             text(*node, "TargetType"), text(*node, "ResourceKey"), theme,
                             scoped});
        }

        // <ResourceDictionary.ThemeDictionaries> is the property element the
        // themes hang under, and each child dictionary's own key -- Default,
        // Light, HighContrast -- names the theme for everything inside it.
        //
        // Anything below an element that is not dictionary machinery -- a
        // control in a template hanging its own <Button.Resources>, a style
        // inside a presenter -- is a local override: the same key, but only
        // for that subtree, and invisible to an application-level lookup.
        std::string_view const name{node->name().chars()};
        bool const themes = name.ends_with(".ThemeDictionaries");
        bool const machinery = name == "ResourceDictionary" || themes ||
                               name.ends_with(".MergedDictionaries");
        for (auto&& child : node->children()) {
            pending.push_back({&child, themes ? text(child, "Key") : theme, scoped || !machinery});
        }
    }
    return found;
}

namespace {

// The text under a node, its runs of white space folded into single spaces.
std::string folded(wxl::xml::node const& node) {
    std::string text;
    bool space = false;
    for (auto const piece : node.text_pieces()) {
        for (char const c : piece.chars()) {
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                space = !text.empty();
                continue;
            }
            if (space) {
                text += ' ';
                space = false;
            }
            text += c;
        }
    }
    return text;
}

}  // namespace

// The file's text, owned and kept, and where each member stands in it. The
// index is an open-addressing table of member numbers: one allocation for the
// whole of it and nothing to sort. Parsed members live in one arena, their
// nodes viewing the text in place.
struct documentation_file::state {
    struct member {
        std::string_view id;
        std::uint32_t begin = 0;  // the member's element, from '<' to after its end tag
        std::uint32_t end = 0;
        wxl::xml::node const* node = nullptr;
        bool parsed = false;
    };

    std::string text;
    wxl::xml::arena arena;
    std::vector<member> members;       // in document order
    std::vector<std::uint32_t> table;  // a member's number plus one, 0 free; a power of two, at most half full
    std::size_t next = 0;              // the first member parse_some() has not looked at

    // The documentation of the members asked for, by member number: only
    // those pay for it, out of the tens of thousands a file holds.
    std::map<std::uint32_t, member_documentation> documented;

    std::size_t home(std::string_view id) const noexcept {
        return std::hash<std::string_view> {}(id) & (table.size() - 1);
    }

    member* find(std::string_view id) noexcept {
        if (table.empty()) {
            return nullptr;
        }
        for (std::size_t at = home(id); table[at]; at = (at + 1) & (table.size() - 1)) {
            if (members[table[at] - 1].id == id) {
                return &members[table[at] - 1];
            }
        }
        return nullptr;
    }

    // The member's element parsed where it stands: the byte after its end tag
    // becomes the zero the parser stops at, and then is put back. A member
    // that does not parse is left without a node.
    void parse(member& m) {
        m.parsed = true;
        char& after = text[m.end];
        char const kept = after;
        after = '\0';
        try {
            std::string_view const fragment {text.data() + m.begin, m.end - m.begin};
            wxl::xml::validate_utf8(fragment);
            wxl::xml::parser reader {fragment, arena};
            m.node = &reader.parse();
        } catch (wxl::xml::exception const&) {
        }
        after = kept;
    }
};

documentation_file::documentation_file(std::filesystem::path const& file) : state_(std::make_unique<state>()) {
    state& st = *state_;
    st.text = wxl::xml::read_file(file);
    std::string_view const text = st.text;

    // A member is found by its start tag and its end tag, and named by its
    // name attribute -- a documentation ID, which never holds a character a
    // document would have to escape.
    constexpr std::string_view open = "<member";
    constexpr std::string_view close = "</member>";
    for (std::size_t at = text.find(open); at != std::string_view::npos; at = text.find(open, at)) {
        std::size_t const tag_end = text.find('>', at);
        if (tag_end == std::string_view::npos) {
            break;
        }
        char const after = text[at + open.size()];
        if (after != ' ' && after != '\t' && after != '\r' && after != '\n') {
            at = tag_end;  // <members>, or a <member> without attributes
            continue;
        }
        std::string_view const tag = text.substr(at, tag_end + 1 - at);
        std::size_t end = tag_end + 1;
        if (!tag.ends_with("/>")) {
            std::size_t const closing = text.find(close, tag_end);
            if (closing == std::string_view::npos) {
                break;
            }
            end = closing + close.size();
        }

        std::size_t const name = tag.find("name=");
        if (name != std::string_view::npos && name + 5 < tag.size()) {
            std::size_t const value_end = tag.find(tag[name + 5], name + 6);
            if (value_end != std::string_view::npos) {
                st.members.push_back({tag.substr(name + 6, value_end - name - 6), static_cast<std::uint32_t>(at),
                                      static_cast<std::uint32_t>(end)});
            }
        }
        at = end;
    }

    st.table.assign(std::bit_ceil(2 * st.members.size() + 1), 0);
    for (std::uint32_t number = 0; number < st.members.size(); ++number) {
        std::string_view const id = st.members[number].id;
        std::size_t at = st.home(id);
        while (st.table[at] && st.members[st.table[at] - 1].id != id) {
            at = (at + 1) & (st.table.size() - 1);
        }
        st.table[at] = number + 1;  // a later member of the same ID wins
    }
}

documentation_file::documentation_file(documentation_file&&) noexcept = default;
documentation_file& documentation_file::operator=(documentation_file&&) noexcept = default;
documentation_file::~documentation_file() = default;

member_documentation const* documentation_file::find(std::string_view id) {
    state::member* const member = state_->find(id);
    if (!member) {
        return nullptr;
    }
    if (!member->parsed) {
        state_->parse(*member);
    }
    if (!member->node) {
        return nullptr;
    }

    auto const number = static_cast<std::uint32_t>(member - state_->members.data());
    auto const [entry, added] = state_->documented.try_emplace(number);
    if (!added) {
        return &entry->second;
    }

    member_documentation& documentation = entry->second;
    for (auto&& part : member->node->children()) {
        std::string_view const kind {part.name().chars()};
        if (kind == "summary") {
            documentation.summary = folded(part);
        } else if (kind == "returns") {
            documentation.returns = folded(part);
        } else if (kind == "deprecated") {
            documentation.deprecated = folded(part);
        } else if (kind == "param") {
            auto const param = part.attribute("name");
            documentation.params.emplace_back(param ? std::string {param->chars()} : std::string {}, folded(part));
        }
    }
    return &documentation;
}

bool documentation_file::parse_some(std::chrono::steady_clock::time_point deadline) {
    state& st = *state_;
    while (st.next < st.members.size()) {
        state::member& member = st.members[st.next++];
        // Parsed already by find(), at once, when it was asked for.
        if (member.parsed) {
            continue;
        }
        st.parse(member);
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }
    while (st.next < st.members.size() && st.members[st.next].parsed) {
        ++st.next;
    }
    return st.next < st.members.size();
}

std::size_t documentation_file::size() const noexcept {
    return state_->members.size();
}
