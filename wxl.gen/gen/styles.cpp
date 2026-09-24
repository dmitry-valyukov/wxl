#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"
#include "xml_input.h"

import std;

// The framework's named styles and brushes, as paths the builder syntax can
// write:
//
//     TextBlock { L"Title", styles.TextBlock.Title }
//     Border { background = brushes.Card.BackgroundFillColor.Default }
//
// This is the one artefact that does not come from metadata. A style is a
// resource, and metadata knows nothing about resources -- what declares it is
// the XAML the framework ships, where every named style is a <Style> element
// carrying an x:Key and the TargetType it applies to. That TargetType is why
// the grouping needs no guesswork: the document says which control a style is
// for, so `TitleTextBlockStyle` is filed under TextBlock because it says so,
// not because its name happens to end in one.
//
// The output is bounded by the profile like everything else: a style whose
// target type was not generated is dropped, since there is no control to
// apply it to.
//
// Nothing is emitted per resource. Every path element is an empty type
// carrying an index, and one function behind them all does the lookup, so 200
// styles cost 200 pointers of static data and no code -- rather than the pair
// of functions and the cache slot each would need of its own.
//
// That one function is not written here. Only two things about styles and
// brushes depend on what a dictionary declares -- the list of paths and the
// list of keys behind them -- and those are all this file emits, into
// `styles.h` / `style_names.h` and `brushes.h` / `brush_names.h`. The lookup
// itself, the cache and its teardown are ordinary code that never varies with
// the input, so they are hand-written in wxl.ui: src/style_lookup.h,
// src/brush_lookup.h and src/resource_lookup.cpp. Constant text has no
// business inside a generator, where nobody can read it under the escaped
// braces.

using namespace md;

namespace gen {
namespace {

// One named style, as the dictionary declares it.
struct style {
    std::string key;     // TitleTextBlockStyle -- the framework's own name
    std::string target;  // TextBlock -- the type it applies to
};

// A style named after nothing but its own target type: `AppBarButtonStyle`
// for AppBarButton, which is the type's plain, explicitly named default.
constexpr std::string_view default_style_name = "Standard";

// Namespace-qualified target types are written with the prefix the document
// declared (`controls:InfoBadge`); prefixes are not resolved, and the bare
// name is what a wxl type is called anyway.
std::string_view unprefixed(std::string_view name) {
    auto const colon = name.rfind(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

// What is left of a key once it stops repeating what the grouping already
// says: TitleTextBlockStyle under TextBlock is `Title`.
//
// PascalCase, against wxl's rule that a member is camelCase, and for the
// reason that rule exists: it keeps a type and a property tag from competing
// for one name. Nothing competes here -- `styles` carries resources, never
// tags -- and both halves of a path are names the framework owns: the group
// is a TargetType, the leaf what is left of a resource key. A path reads as
// a quotation from WinUI, so it is spelled the way WinUI spells it. It also
// costs nothing to keep clear of the keywords: DefaultButtonStyle is
// `Default`, where camelCase had to write `default_`.
std::string style_member_name(std::string_view key, std::string_view target) {
    std::string name{key};
    if (name.ends_with("Style")) {
        name.resize(name.size() - std::string_view{"Style"}.size());
    }
    if (auto const at = name.find(target); at != std::string::npos) {
        name.erase(at, target.size());
    }
    if (name.empty()) {
        return std::string{default_style_name};
    }
    return name;
}

// The two vocabularies of brush keys the framework's dictionaries carry.
// `<X>ThemeBrush` is the older, Windows 8 one and it is a different language,
// not a decoration: it names a control part with the *state before the part*
// (AppBarItemPointerOverBackgroundThemeBrush), where `<X>Brush` puts the
// state last (ListViewItemBackgroundPointerOverBrush). In the dictionaries
// shipped with WinUI 2.4 the split is clean -- 350 keys of the first kind
// against 405 of the second, and not one counterexample to the word order --
// so the two get roots of their own: `themeBrushes.appBarItemPointerOverBackground`
// rather than a `brushes.` path with `Theme` left dangling off the end.
constexpr std::string_view theme_brush_suffix = "ThemeBrush";
constexpr std::string_view brush_suffix = "Brush";

// The word boundaries of a PascalCase name, as offsets where a word starts.
// An acronym run counts as one word, and the capital that begins the next
// one is not part of it: IMECandidateWindow is IME + Candidate + Window.
std::vector<size_t> word_starts(std::string_view name) {
    std::vector<size_t> starts;
    for (size_t i = 0; i < name.size(); ++i) {
        bool const upper = std::isupper(static_cast<unsigned char>(name[i])) != 0;
        if (i == 0) {
            starts.push_back(0);
            continue;
        }
        if (!upper) {
            continue;
        }
        // A capital starts a word unless it is inside an acronym run --
        // and the last capital of a run is the exception to that, because
        // the lowercase after it belongs to the word it begins.
        bool const after_upper = std::isupper(static_cast<unsigned char>(name[i - 1])) != 0;
        bool const before_lower =
            i + 1 < name.size() && std::islower(static_cast<unsigned char>(name[i + 1])) != 0;
        if (!after_upper || before_lower) {
            starts.push_back(i);
        }
    }
    return starts;
}

// Words a group name may not end on. Two kinds, and one list because the
// consequence is the same.
//
// A state is the leaf's business and never the group's: without this,
// `CheckBoxPointerOver` would be a group of three and `.Background` would
// name a brush, when what the reader wants under CheckBox is
// `.PointerOverBackground` said whole.
//
// The rest are halves of a compound the next word finishes. `Over` is a word
// of its own to a PascalCase reader, so `PointerOver` would end a group on
// it; `LayerOnMicaBaseAlt` would give a group called `LayerOn`.
//
// Parts -- Background, Foreground, Fill -- are deliberately *not* here. A
// part is a perfectly good group when its own name is taken (SystemControl
// has 36 Background keys and no plain `SystemControlBackground`), and where
// it is not a group, the rule that a group may not take a name a brush
// already has keeps it out: `ListViewItemBackground` is itself a key.
bool no_group_ends_on(std::string_view word) {
    static constexpr std::string_view words[] = {
        "Pointer", "Over",     "Pressed", "Disabled",      "Checked", "Unchecked",
        "Selected", "Focused", "Indeterminate", "On", "In", "Alt",    "Out"};
    return std::ranges::find(words, word) != std::ranges::end(words);
}

// What a level's keys say about one word-boundary prefix: how many of them
// start with it, and which words they go on with. The second is what
// distinguishes a name that divides here from one that merely passes through.
struct Prefix {
    size_t keys = 0;
    std::map<std::string, size_t> next;  // continuation -> how many keys take it

    // Whether a level made here would be worth its dot. It is not when one
    // continuation takes nearly all of the keys: the prefix is then the
    // middle of a name rather than the place it divides, and the few keys
    // that go elsewhere are too few to be a group of their own anyway.
    bool divides_here() const {
        size_t most = 0;
        for (auto&& [word, count] : next) {
            most = std::max(most, count);
        }
        return most * 4 < keys * 3;
    }
};

// Where a key's name splits into a group and a leaf, or npos for a key that
// stays at the root.
//
// A word-boundary prefix qualifies as a group if it names a type the profile
// generated -- ComboBox, ListViewItem -- or if at least three keys in the
// same root share it, which is what catches the controls a profile leaves out
// (ScrollBar, FlipView, IMECandidate) and the parts no type is named after.
//
// Between two qualifying prefixes the shorter is the group, and the longer
// takes over on either of two conditions.
//
// It names a type and the shorter does not: `Toggle` is shared by every
// ToggleButton and ToggleSwitch key, but no such control exists -- it is the
// word the two type names happen to begin with -- while ToggleButton is a
// type, so that is the group. `List` and ListBox/ListView are the same story.
//
// Or the shorter prefix has only one continuation, which is what tells a
// boundary from the middle of a name. Every one of the 33 keys under the
// generated type `Grid` goes on with the word `View`, so `Grid` is not where
// the name divides -- it is a spelling accident, and GridViewItem takes over.
// `Card` continues two ways, into Background and into Stroke, so it is a real
// boundary and stays the group: brushes.Card.BackgroundFillColorDefault and
// brushes.Card.StrokeColorDefault, not a pair of unrelated roots. Likewise
// ComboBox, which continues twenty ways, keeps ComboBoxArrow in its leaf.
size_t group_split(std::string_view stem, std::set<std::string> const& generated,
                   std::map<std::string, Prefix, std::less<>> const& shared,
                   std::set<std::string, std::less<>> const& taken) {
    auto const starts = word_starts(stem);
    size_t best = std::string_view::npos;
    bool best_divides = false;
    bool best_is_type = false;
    for (size_t w = 1; w < starts.size(); ++w) {
        auto const cut = starts[w];
        if (no_group_ends_on(stem.substr(starts[w - 1], cut - starts[w - 1]))) {
            continue;
        }
        auto const prefix = stem.substr(0, cut);
        // A brush of that very name is already a member here, and one member
        // cannot be both a brush and a group: ListViewItemBackground is a key
        // of its own, so `Background` stays a leaf and its seven states stay
        // beside it as BackgroundPointerOver and the rest.
        if (taken.contains(prefix)) {
            continue;
        }
        auto const here = shared.find(prefix);
        if (here == shared.end()) {
            continue;
        }
        bool const is_type = generated.count(std::string{prefix}) != 0;
        if (!is_type && here->second.keys < 3) {
            continue;
        }
        if (best == std::string_view::npos || (is_type && !best_is_type) || !best_divides) {
            best = cut;
            best_divides = here->second.divides_here();
            best_is_type = is_type;
        }
    }
    return best;
}

// One root of brush paths: a struct per group, then the root's own struct
// carrying the groups and whatever stayed flat. `index` walks on across
// roots, because they share one name table.
// One level of the tree: what stayed a brush here, and what became a group.
struct BrushNode {
    std::map<std::string, BrushNode> groups;    // group name -> the level under it
    std::map<std::string, std::string> leaves;  // leaf -> the framework's key
};

// Split a level into groups and leaves, then split each group the same way.
// The remainder handed down is strictly shorter than what came in, so the
// recursion ends on its own -- SystemControl's 170 keys go to Highlight, to
// Background, and no further than their words allow.
BrushNode build_brush_tree(std::map<std::string, std::string> const& stems,
                           std::set<std::string> const& generated) {
    // How many keys of this level share each word-boundary prefix, which is
    // the second of the two signals group_split() weighs. Counted per level:
    // under SystemControl it is the 170 keys there that vote, not all 755.
    std::map<std::string, Prefix, std::less<>> shared;
    std::set<std::string, std::less<>> taken;
    for (auto&& [stem, key] : stems) {
        taken.insert(stem);
        auto const starts = word_starts(stem);
        for (size_t w = 1; w < starts.size(); ++w) {
            auto& prefix = shared[stem.substr(0, starts[w])];
            ++prefix.keys;
            auto const end = w + 1 < starts.size() ? starts[w + 1] : stem.size();
            ++prefix.next[std::string{stem.substr(starts[w], end - starts[w])}];
        }
    }

    std::map<std::string, std::map<std::string, std::string>> groups;  // group -> stem -> key
    BrushNode node;
    for (auto&& [stem, key] : stems) {
        auto const cut = group_split(stem, generated, shared, taken);
        if (cut == std::string_view::npos) {
            node.leaves.emplace(stem, key);
        } else {
            groups[stem.substr(0, cut)].emplace(stem.substr(cut), key);
        }
    }

    // What is left of a grouping too thin to be one goes back to this level.
    // A group of one is only a longer way to write the same name, and a group
    // named after no type needs three keys to have earned its name -- two
    // `ListPickerFlyoutPresenter*` keys are not a control called `List`.
    for (auto&& [group, members] : groups) {
        if (members.size() < (generated.count(group) ? 2u : 3u)) {
            for (auto&& [stem, key] : members) {
                node.leaves.emplace(group + stem, key);
            }
            continue;
        }
        node.groups.emplace(group, build_brush_tree(members, generated));
    }
    return node;
}

// The tree as C++: a struct per level, children before the parent because a
// member needs its type declared. `path` is what the enclosing groups were
// called, which is what keeps SystemControlHighlightBrushes from colliding
// with any other Highlight.
void write_brush_node(std::ostream& header, BrushNode const& node, std::string_view path,
                      std::string_view type, std::string_view group_suffix, size_t& index,
                      std::vector<std::string>& names) {
    for (auto&& [group, child] : node.groups) {
        auto const child_path = std::string{path} + group;
        write_brush_node(header, child, child_path, child_path + std::string{group_suffix},
                         group_suffix, index, names);
    }

    std::print(header, "\nstruct {} {{\n", type);
    for (auto&& [group, child] : node.groups) {
        std::print(header, "    static constexpr {}{}{} {}{{}};\n", path, group, group_suffix,
                   group);
    }
    for (auto&& [leaf, key] : node.leaves) {
        std::print(header, "    static constexpr resources::BrushOf<{}> {}{{}};  // {}\n", index++,
                   leaf, key);
        names.push_back(key);
    }
    std::print(header, "}};\n");
}

// One root of brush paths, from the keys down to the object written in front
// of them. `index` walks on across roots, because they share one name table.
void write_brush_paths(std::ostream& header, std::string_view group_suffix,
                       std::string_view paths_type, std::string_view object,
                       std::map<std::string, std::string> const& stems,
                       std::set<std::string> const& generated, size_t& index,
                       std::vector<std::string>& names) {
    auto const tree = build_brush_tree(stems, generated);
    write_brush_node(header, tree, "", paths_type, group_suffix, index, names);
    std::print(header, "\ninline constexpr {} {};\n", paths_type, object);
}

// The framework's own keys, in the order the path header numbers them. A
// header of its own because exactly one translation unit reads it --
// src/resource_lookup.cpp, where the lookup lives -- and several hundred
// wide-string literals have no business in a header every user of a path
// includes.
void write_name_table(Output const& out, Emitted& emitted, std::string_view file,
                      std::string_view array, std::vector<std::string> const& names) {
    auto table = open_output(out.dir / file);
    std::print(table, "{}#pragma once\n\nnamespace wxl::resources {{\n\n", banner);
    std::print(table, "inline constexpr wchar_t const* {}[] = {{\n", array);
    for (auto&& name : names) {
        std::print(table, "    L\"{}\",\n", name);
    }
    std::print(table, "}};\n\n}}  // namespace wxl::resources\n");
    emitted.add(file);
}

}  // namespace

void write_styles(Output const& out, Model const& model,
                  std::vector<DictionaryResource> const& resources, Emitted& emitted) {
    // Unscoped only: a style declared inside a template's own resources is
    // that template's business, and the run-time lookup behind style_at()
    // could never reach it anyway.
    std::vector<style> found;
    for (auto&& declared : resources) {
        if (declared.type == "Style" && !declared.target_type.empty() && !declared.scoped) {
            found.push_back({declared.key, std::string{unprefixed(declared.target_type)}});
        }
    }

    // Only styles for controls that were generated: without the control there
    // is nothing to apply the style to.
    std::set<std::string> generated;
    for (auto&& type : model.classes) {
        generated.insert(std::string{type.TypeName()});
    }

    // And of those, the ones the profiles chose. The document names a target
    // by its bare name and the profile by the full one; the generated class
    // joins the two.
    std::map<std::string, MemberFilter const*> chosen;  // target -> its filter
    for (auto&& type : model.classes) {
        auto const filter =
            model.styles.find(std::format("{}.{}", type.TypeNamespace(), type.TypeName()));
        if (filter != model.styles.end()) {
            chosen.emplace(std::string{type.TypeName()}, &filter->second);
        }
    }

    // Grouped by target type, and each group's members sorted, so the header
    // reads as a list rather than as the order a 3 MB document happened to
    // declare things in.
    std::map<std::string, std::map<std::string, std::string>> groups;  // target -> member -> key
    std::set<std::string> seen;
    size_t dropped = 0;
    size_t left_out = 0;
    size_t collisions = 0;
    for (auto&& [key, target] : found) {
        // A dictionary declares each key once per theme, and the lookup at
        // run time is by name -- the framework picks the theme itself, so the
        // second and third declarations of a key are the same resource.
        if (!seen.insert(key).second) {
            continue;
        }
        if (!generated.count(target)) {
            ++dropped;
            continue;
        }
        if (auto const filter = chosen.find(target);
            filter != chosen.end() && !filter->second->allows(key)) {
            ++left_out;
            continue;
        }
        auto& members = groups[target];
        if (!members.emplace(style_member_name(key, target), key).second) {
            ++collisions;
        }
    }

    if (groups.empty()) {
        return;
    }

    // The index a path element carries is the resource's position in the name
    // table, so the two are numbered by one walk over the groups.
    std::vector<std::string> names;
    std::map<std::string, std::map<std::string, size_t>> indices;
    for (auto&& [target, members] : groups) {
        for (auto&& [member, key] : members) {
            indices[target][member] = names.size();
            names.push_back(key);
        }
    }

    {
        auto header = open_output(out.dir / "styles.h");
        std::print(header, "{}#pragma once\n\n#include \"style_lookup.h\"\n\nnamespace wxl::dsl {{\n",
                   banner);

        for (auto&& [target, members] : groups) {
            std::print(header, "\nstruct {}Styles {{\n", target);
            for (auto&& [member, key] : members) {
                std::print(header, "    static constexpr resources::StyleOf<{}> {}{{}};  // {}\n",
                           indices[target][member], member, key);
            }
            std::print(header, "}};\n");
        }

        std::print(header, "\nstruct StylePaths {{\n");
        for (auto&& [target, members] : groups) {
            std::print(header, "    static constexpr {0}Styles {0}{{}};\n", target);
        }
        std::print(header,
                   "}};\n\ninline constexpr StylePaths styles;\n\n}}  // namespace wxl::dsl\n");
        emitted.add("styles.h");
    }

    write_name_table(out, emitted, "style_names.h", "style_names", names);

    std::print("wrote {}styles.h ({} styles in {} groups, {} dropped for a target type that is "
               "not generated{}{})\n",
               out.dir.string() + "\\", names.size(), groups.size(), dropped,
               left_out ? std::format(", {} left out by the profile", left_out) : "",
               collisions ? std::format(", {} keys collided on one name", collisions) : "");
}

void write_brushes(Output const& out, Model const& model,
                   std::vector<DictionaryResource> const& resources, Emitted& emitted) {
    // Concrete brush elements only -- SolidColorBrush, AcrylicBrush,
    // LinearGradientBrush; the suffix is what they have in common. The
    // StaticResource aliases the dictionary also carries (ButtonBackground
    // and the rest of the per-control-part keys) stay out deliberately:
    // they exist to be *overridden* by templates, tripling the surface to
    // read them has no caller yet, and their type is only known by chasing
    // the alias chain.
    std::set<std::string> generated;
    for (auto&& type : model.classes) {
        generated.insert(std::string{type.TypeName()});
    }

    std::map<std::string, std::string> stems;        // stem -> key
    std::map<std::string, std::string> theme_stems;  // the same, for *ThemeBrush
    std::set<std::string> seen;
    size_t left_out = 0;
    size_t collisions = 0;
    for (auto&& declared : resources) {
        // Unscoped only, and for a harder reason than tidiness: a brush
        // declared inside a template's resources is invisible to the
        // application-level lookup behind brush_at(), so a path to one
        // would compile and then throw on first use.
        if (!declared.type.ends_with("Brush") || declared.scoped) {
            continue;
        }
        // A dictionary declares each key once per theme; the lookup at run
        // time is by name and the framework picks the theme itself.
        if (!seen.insert(declared.key).second) {
            continue;
        }
        if (!model.brushes.allows(declared.key)) {
            ++left_out;
            continue;
        }
        // CardBackgroundFillColorDefaultBrush reads as
        // brushes.Card.BackgroundFillColor.Default: the trailing word repeats
        // what the path already says, and the first is the group.
        std::string name{declared.key};
        auto& into = name.ends_with(theme_brush_suffix) ? theme_stems : stems;
        for (auto&& suffix : {theme_brush_suffix, brush_suffix}) {
            if (name.ends_with(suffix)) {
                name.resize(name.size() - suffix.size());
                break;
            }
        }
        if (!into.emplace(std::move(name), declared.key).second) {
            ++collisions;
        }
    }

    if (stems.empty() && theme_stems.empty()) {
        return;
    }

    std::vector<std::string> names;
    {
        auto header = open_output(out.dir / "brushes.h");
        std::print(header, "{}#pragma once\n\n#include \"brush_lookup.h\"\n\nnamespace wxl::dsl {{\n",
                   banner);
        // One index space over both roots, in the order they are written, so
        // a single name table serves them.
        size_t index = 0;
        write_brush_paths(header, "Brushes", "BrushPaths", "brushes", stems, generated, index, names);
        write_brush_paths(header, "ThemeBrushes", "ThemeBrushPaths", "themeBrushes", theme_stems,
                          generated, index, names);
        std::print(header, "\n}}  // namespace wxl::dsl\n");
        emitted.add("brushes.h");
    }

    write_name_table(out, emitted, "brush_names.h", "brush_names", names);

    std::print("wrote {}brushes.h ({} brushes, {} theme brushes{}{})\n", out.dir.string() + "\\",
               stems.size(), theme_stems.size(),
               left_out ? std::format(", {} left out by the profile", left_out) : "",
               collisions ? std::format(", {} keys collided on one name", collisions) : "");
}

void write_alias_index(Output const& out, std::vector<DictionaryResource> const& resources,
                       Emitted& emitted) {
    // Key -> the target each theme declares, in the order the themes appear.
    // Most aliases agree across themes and print as one line; the rest --
    // typically HighContrast pointing at the SystemControl* palette -- print
    // each distinct target with the themes that chose it.
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> aliases;
    std::set<std::pair<std::string, std::string>> seen;  // key + theme
    for (auto&& declared : resources) {
        // The global wiring only: a template's local re-pointing of the same
        // keys (an InfoBar dressing its close button as an app-bar button)
        // would say the opposite of what the top level says.
        if (declared.type != "StaticResource" || declared.alias_for.empty() || declared.scoped) {
            continue;
        }
        if (!seen.emplace(declared.key, declared.theme).second) {
            continue;
        }
        aliases[declared.key].emplace_back(declared.theme, declared.alias_for);
    }

    if (aliases.empty()) {
        return;
    }

    auto file = open_output(out.dir / "aliases.txt");
    std::print(file, R"(// Generated by winui-srcgen. Do not edit by hand.
//
// The StaticResource aliases of the framework's theme dictionaries: the key
// every control part looks its resource up by, and the base resource it
// forwards to -- which is how a control's styling is actually chosen, and
// what to override in an application's own resources to restyle one control
// without touching the palette. Reference only: these keys are not exposed
// as brush paths, brushes.h carries the concrete resources they point to.
//
// One line per key where every theme forwards to the same place; otherwise
// each distinct target is listed with the themes that chose it.

)");

    for (auto&& [key, targets] : aliases) {
        // The distinct targets, each keeping the list of themes that named
        // it, in declaration order.
        std::vector<std::pair<std::string, std::vector<std::string>>> distinct;
        for (auto&& [theme, target] : targets) {
            auto const known = std::find_if(distinct.begin(), distinct.end(),
                                            [&](auto const& entry) { return entry.first == target; });
            if (known == distinct.end()) {
                distinct.push_back({target, {theme}});
            } else {
                known->second.push_back(theme);
            }
        }

        if (distinct.size() == 1) {
            std::print(file, "{}: {}\n", key, distinct.front().first);
            continue;
        }
        std::print(file, "{}:", key);
        for (auto&& [target, themes] : distinct) {
            std::string list;
            for (auto&& theme : themes) {
                list += list.empty() ? theme : ", " + theme;
            }
            std::print(file, "{}{} ({})", &target == &distinct.front().first ? " " : "; ", target,
                       list);
        }
        std::print(file, "\n");
    }
    emitted.add("aliases.txt");

    std::print("wrote {}aliases.txt ({} alias keys)\n", out.dir.string() + "\\", aliases.size());
}

}  // namespace gen
