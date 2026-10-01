#pragma once

// Resource -- one entry of a ResourceDictionary: a key and the value under it.
//
// A dictionary's values are boxed objects of any type; the ones a page
// overrides in practice are a size, a length, a flag, a colour or a word, and
// those are the alternatives. The dictionary takes the value in
// as the framework's own type (impl/resource.cpp), so a Thickness is a
// Thickness there and a Margin lookup finds it.
//
//     ResourceDictionary {
//         entry = Resource {u"TextBoxTopHeaderMargin", Thickness {0, 2, 0, 2}},
//     }

#include "Color.h"
#include "CornerRadius.h"
#include "generated/Microsoft.UI.Xaml.Enums.h"
#include "Thickness.h"
#include "core.h"
#include "hstring_param.h"

namespace wxl {

struct Resource {
    using Value = std::variant<bool, double, hstring, Thickness, CornerRadius, Color, HorizontalAlignment>;

    hstring key;
    Value value;

    // An integer is a length too, and a literal is the text of its
    // character type; without these the variant would take both as a bool.
    Resource(hstring_param const& key, bool value) : key(key), value(value) {}
    Resource(hstring_param const& key, int value) : key(key), value(static_cast<double>(value)) {}
    Resource(hstring_param const& key, double value) : key(key), value(value) {}
    Resource(hstring_param const& key, hstring_param const& text) : key(key), value(hstring(text)) {}
    Resource(hstring_param const& key, Thickness const& value) : key(key), value(value) {}
    Resource(hstring_param const& key, CornerRadius const& value) : key(key), value(value) {}
    Resource(hstring_param const& key, Color value) : key(key), value(value) {}
    Resource(hstring_param const& key, HorizontalAlignment value) : key(key), value(value) {}
};

}  // namespace wxl
