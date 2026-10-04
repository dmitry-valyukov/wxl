#pragma once

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

#include "Object.h"
#include "hstring_param.h"

namespace wxl {

Object loadXaml(hstring_param const& xaml);

}  // namespace wxl
