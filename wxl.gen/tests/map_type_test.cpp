#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "wxl.gen.h"

namespace {

std::filesystem::path const profiles_dir{WXL_GEN_PROFILES_DIR};

// The metadata the rich profile names, read once for the whole suite; the
// type map goes in first, since the mapping asks it which types are projected.
md::cache const& metadata() {
    static std::vector<std::string> const files = [] {
        use_type_map(load_type_map(profiles_dir / "types.json"));
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

// How a property's type, as its interface declares it, maps. The signature is
// held by name: the type it hands out points into it.
gen::TypeUse map_property(std::string_view interface_name, std::string_view property,
                          gen::TypeIndex const& index) {
    for (auto&& candidate : type(interface_name).PropertyList()) {
        if (candidate.Name() == property) {
            md::PropertySig const sig = candidate.Type();
            return gen::map_type(sig.Type(), index);
        }
    }
    throw std::runtime_error(std::format("{} declares no {}", interface_name, property));
}

}  // namespace

TEST(map_type, a_primitive_crosses_as_itself) {
    auto const use = map_property("Microsoft.UI.Xaml.IFrameworkElement", "Width", {});
    ASSERT_TRUE(use.supported) << use.reason;
    EXPECT_EQ(use.value_type, "double");
    EXPECT_EQ(use.param_type, "double");
}

TEST(map_type, a_reference_to_a_value_is_nullable) {
    auto const use =
        map_property("Microsoft.UI.Xaml.Controls.Primitives.IToggleButton", "IsChecked", {});
    ASSERT_TRUE(use.supported) << use.reason;
    EXPECT_EQ(use.value_type, "core::nullable<bool>");
    EXPECT_EQ(use.param_type, "core::nullable<bool> const&");
}

TEST(map_type, an_enum_is_found_in_the_index) {
    auto const unknown = map_property("Microsoft.UI.Xaml.IUIElement", "Visibility", {});
    EXPECT_FALSE(unknown.supported);
    EXPECT_EQ(unknown.reason, "no wxl type for Microsoft.UI.Xaml.Visibility");

    gen::TypeIndex index;
    index.names.emplace(type("Microsoft.UI.Xaml.Visibility"), "Visibility");
    index.headers.emplace(type("Microsoft.UI.Xaml.Visibility"), "Microsoft.UI.Xaml.Enums.h");
    auto const use = map_property("Microsoft.UI.Xaml.IUIElement", "Visibility", index);
    ASSERT_TRUE(use.supported) << use.reason;
    EXPECT_EQ(use.value_type, "Visibility");
    EXPECT_EQ(use.param_type, "Visibility");
    EXPECT_EQ(use.to_winrt, "static_cast<winrt::Microsoft::UI::Xaml::Visibility>($)");
}

// UIElementCollection exists in metadata only to name an IVector<UIElement>,
// so it maps onto the collection of its element.
TEST(map_type, a_collection_class_is_a_collection_of_its_element) {
    gen::TypeIndex index;
    index.names.emplace(type("Microsoft.UI.Xaml.UIElement"), "UIElement");
    index.headers.emplace(type("Microsoft.UI.Xaml.UIElement"), "Microsoft.UI.Xaml.h");

    auto const use = map_property("Microsoft.UI.Xaml.Controls.IPanel", "Children", index);
    ASSERT_TRUE(use.supported) << use.reason;
    EXPECT_TRUE(use.is_collection);
    EXPECT_EQ(use.element_type, "UIElement");
    EXPECT_EQ(use.value_type, "Collection<UIElement>");
}
