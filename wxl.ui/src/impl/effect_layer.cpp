// wxl::impl::insert_layer, when_drawn -- see effect_layer.h.

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <functional>
#include <memory>

#include "impl/effect_layer.h"

namespace wxl::impl {

namespace composition = winrt::Microsoft::UI::Composition;

namespace {

// A name a property set accepts: an identifier, no dots.
constexpr wchar_t const* zKey = L"wxlZIndex";

int z_of(composition::Visual const& visual) {
    float value = 0.0f;
    auto const found = visual.Properties().TryGetScalar(zKey, value);
    return found == composition::CompositionGetValueStatus::Succeeded ? static_cast<int>(value) : 0;
}

}  // namespace

void insert_layer(composition::VisualCollection const& children, composition::Visual const& layer, int z) {
    layer.Properties().InsertScalar(zKey, static_cast<float>(z));
    // The collection enumerates from the bottom up. The layer goes right
    // under the first sibling that stands higher than it; none, and it goes
    // on top.
    for (composition::Visual const& sibling : children) {
        if (z_of(sibling) > z) {
            children.InsertBelow(layer, sibling);
            return;
        }
    }
    children.InsertAtTop(layer);
}

void when_drawn(composition::ContainerVisual const& host,
                std::function<void(composition::VisualCollection const&)> place) {
    auto const children = host.Children();
    if (children.Count() != 0) {
        place(children);
        return;
    }
    auto const waiting = std::make_shared<winrt::event_token>();
    *waiting = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
        [children, place = std::move(place), waiting](auto&&, auto&&) {
            if (children.Count() == 0) return;
            place(children);
            winrt::Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(*waiting);
        });
}

}  // namespace wxl::impl
