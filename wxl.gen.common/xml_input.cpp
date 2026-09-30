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

std::vector<DictionaryResource> dictionary_resources(std::filesystem::path const& dictionary) {
    std::vector<DictionaryResource> found;

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

// The document and an index of its members: an open-addressing table in one
// vector, keyed by views of the IDs into the document -- one allocation for the
// whole index, nothing to sort, and no member's text copied until asked for.
struct DocumentationFile::State {
    struct Slot {
        std::string_view id;
        wxl::xml::node const* member = nullptr;  // null: the slot is free
    };

    wxl::xml::document document;
    std::vector<Slot> slots;  // a power of two, at most half full
    std::size_t size = 0;

    std::size_t home(std::string_view id) const noexcept { return std::hash<std::string_view> {}(id) & (slots.size() - 1); }

    Slot const* find(std::string_view id) const noexcept {
        if (slots.empty()) {
            return nullptr;
        }
        for (std::size_t at = home(id);; at = (at + 1) & (slots.size() - 1)) {
            if (!slots[at].member) {
                return nullptr;
            }
            if (slots[at].id == id) {
                return &slots[at];
            }
        }
    }
};

DocumentationFile::DocumentationFile(std::filesystem::path const& file) : state_(std::make_unique<State>()) {
    auto const* members = state_->document.load_file(file).find("members");
    if (!members) {
        return;
    }
    State& state = *state_;
    state.slots.resize(std::bit_ceil(2 * static_cast<std::size_t>(std::ranges::distance(members->children())) + 1));
    for (auto&& member : members->children_named("member")) {
        auto const name = member.attribute("name");
        if (!name) {
            continue;
        }
        std::string_view const id = name->chars();
        std::size_t at = state.home(id);
        while (state.slots[at].member && state.slots[at].id != id) {
            at = (at + 1) & (state.slots.size() - 1);
        }
        state.size += state.slots[at].member ? 0 : 1;
        state.slots[at] = {id, &member};
    }
}

DocumentationFile::DocumentationFile(DocumentationFile&&) noexcept = default;
DocumentationFile& DocumentationFile::operator=(DocumentationFile&&) noexcept = default;
DocumentationFile::~DocumentationFile() = default;

std::optional<MemberDocumentation> DocumentationFile::find(std::string_view id) const {
    auto const* const found = state_->find(id);
    if (!found) {
        return std::nullopt;
    }

    MemberDocumentation documentation;
    for (auto&& part : found->member->children()) {
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
    return documentation;
}

std::size_t DocumentationFile::size() const noexcept {
    return state_->size;
}
