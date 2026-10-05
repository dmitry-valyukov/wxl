#pragma once

// What the header and the description of a settings card, and the header of a headered control, count as "no text":
// null or an empty string -- the IsNullOrEmpty of the Windows Community Toolkit's SettingsCard, which then collapses the
// part. A blank part is collapsed and holds nothing, not an empty text.
//
// Private: the projection types are used freely, so this header is one only wxl's own sources ever include.

#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

inline bool is_blank(winrt::Windows::Foundation::IInspectable const& content) {
    if (!content) {
        return true;
    }
    if (auto const value = content.try_as<winrt::Windows::Foundation::IPropertyValue>()) {
        return value.Type() == winrt::Windows::Foundation::PropertyType::String && value.GetString().empty();
    }
    return false;
}

}  // namespace wxl::impl
