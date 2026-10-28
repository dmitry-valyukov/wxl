#pragma once

// The XAML resources of an application: the entries of a ResourceDictionary (Resource,
// ThemeDictionary), the lookup of a key in the application's resources (resourceBrush,
// resourceColor) and XAML text loaded (loadXaml).
//
// Included by the generated wrapper of ResourceDictionary (the profile's cppInclude), so it names
// Brush without including the header that defines it.
//
// Resource -- one entry of a ResourceDictionary: a key and the value under it.
//
// A dictionary's values are boxed objects of any type; the ones a page
// overrides in practice are a size, a length, a flag, a colour or a word, and
// those are the alternatives. The dictionary takes the value in
// as the framework's own type (impl/element_helpers.cpp), so a Thickness is a
// Thickness there and a Margin lookup finds it.
//
//     ResourceDictionary {
//         entry = Resource {u"TextBoxTopHeaderMargin", Thickness {0, 2, 0, 2}},
//     }

#include "Color.h"
#include "Object.h"
#include "CornerRadius.h"
#include <wxl/Microsoft.UI.Xaml.Enums.h>
#include "Thickness.h"
#include "core.h"
#include "hstring_param.h"

namespace wxl {

class Brush;

struct Resource {
    using Value = std::variant<bool, double, hstring, Thickness, CornerRadius, Color,
                               HorizontalAlignment, Object>;

    hstring key;
    Value value;

    // An integer is a length too, and a literal is the text of its
    // character type; without these the variant would take both as a bool.
    Resource(hstring_param const& key, bool value) : key(key), value(value) {}
    Resource(hstring_param const& key, int value) : key(key), value(static_cast<double>(value)) {}
    Resource(hstring_param const& key, double value) : key(key), value(value) {}
    Resource(hstring_param const& key, hstring_param const& text)
        : key(key), value(hstring(text)) {}
    Resource(hstring_param const& key, Thickness const& value) : key(key), value(value) {}
    Resource(hstring_param const& key, CornerRadius const& value) : key(key), value(value) {}
    Resource(hstring_param const& key, Color value) : key(key), value(value) {}
    Resource(hstring_param const& key, HorizontalAlignment value) : key(key), value(value) {}
    // Any object of the framework: a brush, a style, a template.
    Resource(hstring_param const& key, Object const& value) : key(key), value(value) {}
};

// The entries of one theme of a dictionary: what a ResourceDictionary of XAML keeps in ThemeDictionaries under "Light" or
// "Dark". The theme's own entries are found before the dictionary's, whichever theme the element is in.
//
//     ResourceDictionary {
//         themeEntry = ThemeDictionary {u"Light", {Resource {u"TabViewBackground", SolidColorBrush {color = rgb(1, 2, 3)}}}},
//     }
struct ThemeDictionary {
    hstring theme;
    std::vector<Resource> entries;

    ThemeDictionary(hstring_param const& theme, std::vector<Resource> entries)
        : theme(theme), entries(std::move(entries)) {}
};

// wxl::resourceBrush -- a resource of the application as a brush, by its key.
//
// The framework's brushes are in `brushes`, one name each; the colors of the system theme under it --
// SystemAccentColor, SystemChromeLowColor -- are not brushes, and what XAML writes as
// `Fill="{ThemeResource SystemAccentColor}"` is this: the key looked up in the application's resources and
// made a brush if it holds a color. The brush is made once, for the theme the application runs in; it does not
// follow a change of theme afterwards.
//
// A key that is not there is a mistake in the name, and an error.
Brush resourceBrush(hstring_param const& key);

/// The same lookup for a key that holds a color -- a gradient stop takes a color, not a brush.
Color resourceColor(hstring_param const& key);

// wxl::loadXaml -- what XAML text describes, made: the XamlReader.Load of the framework.
//
//     auto const element = loadXaml(u"<Button xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' Content='Hi'/>");
//     if (element.is<UIElement>()) { ... }
//
// For the application whose XAML comes from outside it -- typed by a person into an editor, read from a file --
// and so cannot be written as a description. Text that is not valid XAML makes the call throw; what the exception
// says is read with `failureOf` (Failure.h).
//
// The result is the root object of the text, whatever it is: an element, a ResourceDictionary, a Style.
Object loadXaml(hstring_param const& xaml);

}  // namespace wxl
