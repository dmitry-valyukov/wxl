#pragma once

// The control behind wxl::HeaderedContentControl: a ContentControl with a header over its content (Toolkit's
// HeaderedContentControl). Aggregation as in settings_card.h.
//
// Private: the projection types are used freely, so this header is one only wxl's own sources ever include.

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

struct HeaderedContentControlCore : winrt::Microsoft::UI::Xaml::Controls::ContentControlT<HeaderedContentControlCore> {
    HeaderedContentControlCore();

    void OnApplyTemplate();

    winrt::Windows::Foundation::IInspectable header() const { return header_; }
    void header(winrt::Windows::Foundation::IInspectable const& value);

private:
    winrt::Windows::Foundation::IInspectable header_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter headerPart_{nullptr};
};

}  // namespace wxl::impl
