// Resolving a named style or brush out of the application's resources. The
// two live together because they share the one thing worth explaining: a
// cache filled on first use, and the care its teardown needs.
//
// The tables they index are generated -- `style_names.h` and `brush_names.h`
// are the framework's own keys, in the order the path headers number them --
// and this file is the only translation unit that reads them, which is why
// they are headers of their own rather than part of styles.h and brushes.h.

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "Microsoft.UI.Xaml.Media.impl.h"
#include "Microsoft.UI.Xaml.impl.h"
#include "brush_lookup.h"
#include "brush_names.h"
#include "style_lookup.h"
#include "style_names.h"

namespace wxl::resources {
namespace {

// Filled on first use and kept for the life of the process. A nullable
// rather than the wrapper itself, so the tables cost no initialization at
// load time: a default-constructed nullable is a constant, while a Style or
// a Brush of its own would try to activate a real WinUI object long before
// the runtime is up. nullable, not std::optional, because both are wrappers:
// the empty state is the null Impl itself, so the tables carry no separate
// flags.
//
// Brushes get one row per lookup flavour: the application default, then
// Light and Dark resolved from the theme dictionaries by name.
wxl::core::nullable<Style> style_cache[std::size(style_names)];
wxl::core::nullable<Brush> brush_cache[3][std::size(brush_names)];

// Release the cached wrappers before the STA pool their Impls come from is
// torn down. The pool's destructor drives these cleanups (a resource, at
// normal priority) and only then frees its pages, so a slot still holding a
// wrapper is never released against memory already returned to the OS --
// which is what these static tables' own destruction, at process exit past
// the pool, would otherwise do.
wxl::core::module_cleanup const clear_caches{+[] {
                                                for (auto& slot : style_cache) slot.reset();
                                                for (auto& row : brush_cache)
                                                    for (auto& slot : row) slot.reset();
                                            },
                                            wxl::core::module_cleanup::cleanup_normal};

// The key resolved inside a named theme dictionary. Theme dictionaries hang
// off the dictionaries that declare them (XamlControlsResources, merged into
// the application's), not off the application's own, so the walk descends
// the merged chain -- in reverse, the way XAML itself resolves: the last
// dictionary merged wins.
winrt::Windows::Foundation::IInspectable themed_lookup(
    winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary,
    winrt::Windows::Foundation::IInspectable const& theme,
    winrt::Windows::Foundation::IInspectable const& name) {
    if (auto const themes = dictionary.ThemeDictionaries()) {
        if (auto const chosen = themes.TryLookup(theme)) {
            if (auto const inner = chosen.try_as<winrt::Microsoft::UI::Xaml::ResourceDictionary>()) {
                if (auto const hit = inner.TryLookup(name)) {
                    return hit;
                }
            }
        }
    }
    auto const merged = dictionary.MergedDictionaries();
    for (uint32_t back = merged.Size(); back != 0; --back) {
        if (auto const hit = themed_lookup(merged.GetAt(back - 1), theme, name)) {
            return hit;
        }
    }
    return nullptr;
}


// A resource under a brush key that is a bare Color rather than a Brush,
// made into the brush the name promises. The framework has such keys of its
// own -- TabViewBorderBrush and NavigationViewExpandedPaneBackground both
// hold a Color -- and as<Brush> on one of those throws, so every path that
// ends in a resource goes through here.
Brush brush_from(winrt::Windows::Foundation::IInspectable const& value) {
    if (auto const colour = value.try_as<winrt::Windows::UI::Color>()) {
        return Object::Impl::wrap<Brush>(winrt::Microsoft::UI::Xaml::Media::Brush{
            winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{*colour}});
    }
    return Object::Impl::wrap<Brush>(value.as<winrt::Microsoft::UI::Xaml::Media::Brush>());
}

}  // namespace

Style const& style_at(int index) {
    auto& slot = style_cache[index];
    if (!slot) {
        auto const resources = winrt::Microsoft::UI::Xaml::Application::Current().Resources();
        slot = Object::Impl::wrap<Style>(
            resources.Lookup(winrt::box_value(winrt::hstring{style_names[index]}))
                .as<winrt::Microsoft::UI::Xaml::Style>());
    }
    return *slot;
}

Brush const& brush_at(int index) {
    auto& slot = brush_cache[0][index];
    if (!slot) {
        auto const resources = winrt::Microsoft::UI::Xaml::Application::Current().Resources();
        slot = brush_from(resources.Lookup(winrt::box_value(winrt::hstring{brush_names[index]})));
    }
    return *slot;
}

Brush const& brush_at(int index, ElementTheme theme) {
    if (theme != ElementTheme::Light && theme != ElementTheme::Dark) {
        return brush_at(index);
    }
    auto& slot = brush_cache[theme == ElementTheme::Light ? 1 : 2][index];
    if (!slot) {
        auto const resources = winrt::Microsoft::UI::Xaml::Application::Current().Resources();
        auto const key = winrt::box_value(winrt::hstring{theme == ElementTheme::Light
                                                             ? L"Light"
                                                             // Dark lives under the key WinUI
                                                             // spells "Default".
                                                             : L"Default"});

        // The *colour*, not the brush. The brush the theme dictionary holds
        // is poisoned for this purpose: its Color comes from a StaticResource
        // reference that the framework resolves against the theme that is
        // *active*, so asking the Light application for the "Default" brush
        // hands back light paint under a dark name. The colour keys are
        // literals (#202020), so they are the one thing in the dictionary
        // that means what it says. The framework's convention pairs them:
        // <X>Brush is a SolidColorBrush of the colour <X>.
        std::wstring_view const brush_name{brush_names[index]};
        if (brush_name.ends_with(L"Brush")) {
            auto const colour = themed_lookup(
                resources, key,
                winrt::box_value(winrt::hstring{brush_name.substr(0, brush_name.size() - 5)}));
            if (colour) {
                if (auto const value = colour.try_as<winrt::Windows::UI::Color>()) {
                    slot = Object::Impl::wrap<Brush>(winrt::Microsoft::UI::Xaml::Media::Brush{
                        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{*value}});
                    return *slot;
                }
            }
        }

        // No colour of that name: the key is theme-invariant, or its brush
        // is no solid colour. The named brush -- themed if a dictionary
        // declares one, the plain application lookup otherwise -- is the
        // best there is.
        auto const name = winrt::box_value(winrt::hstring{brush_names[index]});
        auto const found = themed_lookup(resources, key, name);
        slot = brush_from(found ? found : resources.Lookup(name));
    }
    return *slot;
}

}  // namespace wxl::resources
