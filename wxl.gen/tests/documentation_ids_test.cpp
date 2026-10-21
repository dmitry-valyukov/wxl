#include <gtest/gtest.h>

#include <filesystem>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "documentation.h"

#include "wxl.gen.h"

// The documentation IDs the generator's own collection gives members: keyed on
// the class that owns them, a factory's methods as its constructors, the form
// with a token as the method it is a form of.

namespace {

std::filesystem::path const profiles_dir{WXL_GEN_PROFILES_DIR};

type_map const& types() {
    static type_map const map = load_type_map(profiles_dir / "types.json");
    return map;
}

md::cache const& metadata() {
    static std::vector<std::string> const files = [] {
        auto const profiles = resolve_profiles({profiles_dir / "rich.json"}, default_nuget_root());
        std::vector<std::string> paths;
        for (auto&& file : profiles.metadata) {
            paths.push_back(file.string());
        }
        return paths;
    }();
    static md::cache const db{files};
    return db;
}

md::TypeDef type(std::string_view name) {
    return metadata().find_required(name);
}

// An index naming `wrapped` as the generator would: by bare name, in their
// namespace's header.
gen::type_index index_of(std::initializer_list<std::string_view> wrapped) {
    gen::type_index index{types()};
    for (std::string_view const name : wrapped) {
        auto const def = type(name);
        index.names.emplace(def, std::string{def.TypeName()});
        index.headers.emplace(def, std::string{def.TypeNamespace()} + ".h");
    }
    return index;
}

std::vector<gen::member_info> collect(std::string_view iface, std::string_view owner, bool factory,
                                      std::set<std::string> const& allowed, gen::type_index const& index) {
    std::vector<gen::member_info> members;
    std::vector<gen::skipped_member> skipped;
    gen::collect_interface_members(type(iface), type(owner), factory, allowed, index, members, skipped);
    EXPECT_TRUE(skipped.empty()) << skipped.front().name << ": " << skipped.front().reason;
    return members;
}

std::vector<std::string_view> names(std::initializer_list<std::string_view> list) {
    return list;
}

}  // namespace

// What IButton declares is documented as Button's.
TEST(documentation_ids, a_property_is_keyed_on_the_class) {
    auto const members = collect("Microsoft.UI.Xaml.Controls.IButton", "Microsoft.UI.Xaml.Controls.Button", false,
                                 {"Flyout"}, index_of({"Microsoft.UI.Xaml.Controls.Primitives.FlyoutBase"}));
    ASSERT_EQ(members.size(), 2u);  // the getter and the setter
    for (auto&& member : members) {
        EXPECT_EQ(member.doc_id, "P:Microsoft.UI.Xaml.Controls.Button.Flyout");
    }
}

// A composable factory's method is the constructor, without the pair
// composition adds -- in its ID and in its parameters alike.
TEST(documentation_ids, a_composable_factory_is_a_constructor) {
    auto const members = collect("Microsoft.UI.Xaml.Automation.Peers.IFrameworkElementAutomationPeerFactory",
                                 "Microsoft.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer", true,
                                 {"CreateInstanceWithOwner"},
                                 index_of({"Microsoft.UI.Xaml.FrameworkElement",
                                           "Microsoft.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer"}));
    ASSERT_EQ(members.size(), 1u);
    EXPECT_TRUE(members[0].composable_constructor);
    EXPECT_EQ(members[0].doc_id,
              "M:Microsoft.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer.#ctor(Microsoft.UI.Xaml.FrameworkElement)");
    EXPECT_EQ(gen::parameter_names(members[0]), names({"owner"}));
}

// The form with a token is documented as the method it is a form of, and
// declares the token as a parameter of its own.
TEST(documentation_ids, the_form_with_a_token_keeps_the_methods_id) {
    auto const members = collect("Microsoft.UI.Xaml.Controls.IContentDialog", "Microsoft.UI.Xaml.Controls.ContentDialog",
                                 false, {"ShowAsync"},
                                 index_of({"Microsoft.UI.Xaml.Controls.ContentDialogResult",
                                           "Microsoft.UI.Xaml.Controls.ContentDialogPlacement"}));
    ASSERT_EQ(members.size(), 4u);
    std::string const placed = "M:Microsoft.UI.Xaml.Controls.ContentDialog.ShowAsync("
                               "Microsoft.UI.Xaml.Controls.ContentDialogPlacement)";
    for (auto&& member : members) {
        bool const takes_placement = !member.params.empty() && member.params.front().name == "placement";
        EXPECT_EQ(member.doc_id, takes_placement ? placed : "M:Microsoft.UI.Xaml.Controls.ContentDialog.ShowAsync");
        if (member.under_token) {
            EXPECT_EQ(gen::parameter_names(member),
                      takes_placement ? names({"placement", "stop"}) : names({"stop"}));
        }
    }
}

// A method handing back its out parameter keeps it in the ID, drops it from
// its parameters, and takes its description for the result.
TEST(documentation_ids, an_out_result_is_documented_as_the_result) {
    auto members = collect("Microsoft.UI.Text.ITextRange", "Microsoft.UI.Text.ITextRange", false, {"GetText"},
                           index_of({"Microsoft.UI.Text.TextGetOptions"}));
    ASSERT_EQ(members.size(), 1u);
    auto& get_text = members[0];
    EXPECT_TRUE(get_text.out_result);
    EXPECT_EQ(get_text.doc_id, "M:Microsoft.UI.Text.ITextRange.GetText(Microsoft.UI.Text.TextGetOptions,System.String@)");
    EXPECT_EQ(gen::parameter_names(get_text), names({"options"}));

    documentation_source docs{{L"en"}};
    gen::document(&docs, type("Microsoft.UI.Text.ITextRange"), get_text);
    EXPECT_FALSE(get_text.doc.summary.empty());
    EXPECT_EQ(get_text.doc.returns, "The text in the text range.");
}
