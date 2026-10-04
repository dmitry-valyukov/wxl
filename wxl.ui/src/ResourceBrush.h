#pragma once

// wxl::resourceBrush -- a resource of the application as a brush, by its key.
//
// The framework's brushes are in `brushes`, one name each; the colors of the system theme under it --
// SystemAccentColor, SystemChromeLowColor -- are not brushes, and what XAML writes as
// `Fill="{ThemeResource SystemAccentColor}"` is this: the key looked up in the application's resources and
// made a brush if it holds a color. The brush is made once, for the theme the application runs in; it does not
// follow a change of theme afterwards.
//
// A key that is not there is a mistake in the name, and an error.

#include "Color.h"
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include "hstring_param.h"

namespace wxl {

Brush resourceBrush(hstring_param const& key);

/// The same lookup for a key that holds a color -- a gradient stop takes a color, not a brush.
Color resourceColor(hstring_param const& key);

}  // namespace wxl
