#pragma once

// A spotlight over the pointer, one per XAML island, as RevealEffect's two
// lights and Button3DEffect's share it: the settings of one pointer spot
// light and the spotlights burning with them -- one for each island the light
// has an element in. The island's light follows the pointer by an expression
// the compositor evaluates, comes up when the pointer enters the island and
// goes out when it leaves, and is put out by its intensity, never by
// IsEnabled: a sprite wearing a lighting brush that no enabled light is aimed
// at is drawn white all over.
//
// Included by the effects' .cpp files only, after the projection.

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <vector>

#include "../Color.h"
#include "../core.h"

namespace wxl::impl {

// The spotlight over the pointer of one island, shining straight down. The
// island is named by the element at its root, held weakly. The light listens
// to that root for the pointer coming and going, and stops listening when it
// goes itself: the root holds nothing of the light.
struct island_spot_light {
    winrt::weak_ref<winrt::Microsoft::UI::Xaml::UIElement> root;
    winrt::Microsoft::UI::Composition::CompositionPropertySet pointer{nullptr};
    winrt::Microsoft::UI::Composition::SpotLight light{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement::PointerEntered_revoker entered;
    winrt::Microsoft::UI::Xaml::UIElement::PointerExited_revoker exited;
    winrt::Microsoft::UI::Xaml::UIElement::PointerMoved_revoker moved;
    // The pointer is over the island.
    bool shining = false;
};

// The settings of one spotlight over the pointer and the islands' lights that
// burn with them.
struct pointer_spot_light : core::sta_refcounted {
    // How long an island's light takes to come up and to go out, and what a
    // press does to the spot: three times the light at once, back to its own
    // over the flash.
    static constexpr std::chrono::milliseconds fade_time{180};
    static constexpr std::chrono::milliseconds flash_time{400};
    static constexpr float flash_gain = 3.0f;

    // The height the light stands at, in its own property set: the expression
    // that places it reads it from there, so a press can move the light by
    // animating one scalar.
    static constexpr wchar_t const* height_property = L"Height";

    float size;
    // The disc of full light inside the spot; 0 for a spot that fades from
    // its very axis.
    float full = 0.0f;
    float height = 120.0f;
    float intensity = 1.0f;
    float diffuse;
    Color color = rgb(255, 255, 255);
    float constant_attenuation = 1.0f;
    float linear_attenuation = 0.0f;
    std::vector<island_spot_light> island_lights;

    pointer_spot_light(float size, float diffuse) : size{size}, diffuse{diffuse} {}

    island_spot_light* island_light_of(winrt::Microsoft::UI::Xaml::UIElement const& root) {
        for (island_spot_light& island_light : island_lights) {
            if (island_light.root.get() == root) return &island_light;
        }
        return nullptr;
    }

    // Aims the light of the layer's island at the layer, lighting one up over
    // that island if this is its first.
    void light_up(winrt::Microsoft::UI::Xaml::UIElement const& root,
                  winrt::Microsoft::UI::Composition::Visual const& layer) {
        // An island that is gone takes its light with it.
        std::erase_if(island_lights,
                      [](island_spot_light const& each) { return !each.root.get(); });
        if (island_spot_light const* const known = island_light_of(root)) {
            known->light.Targets().Add(layer);
            return;
        }

        auto const space =
            winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(root);

        island_spot_light island_light;
        island_light.root = root;
        island_light.pointer = winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::
            GetPointerPositionPropertySet(root);
        island_light.light = space.Compositor().CreateSpotLight();
        island_light.light.CoordinateSpace(space);
        island_light.light.Direction({0.0f, 0.0f, -1.0f});
        island_light.light.InnerConeIntensity(0.0f);
        island_light.light.OuterConeIntensity(0.0f);
        island_light.light.Properties().InsertScalar(height_property, height);
        island_light.light.Targets().Add(layer);
        aim(island_light);
        raise(island_light);

        // The handlers hold the light by a bare pointer: they are revoked
        // with the island's light, and that goes no later than the light does.
        island_light.entered =
            root.PointerEntered(winrt::auto_revoke, [this](auto const& sender, auto&&) {
                shine(sender.template as<winrt::Microsoft::UI::Xaml::UIElement>(), true);
            });
        island_light.exited =
            root.PointerExited(winrt::auto_revoke, [this](auto const& sender, auto&&) {
                shine(sender.template as<winrt::Microsoft::UI::Xaml::UIElement>(), false);
            });
        watch(island_light, root);

        island_lights.push_back(std::move(island_light));
    }

    // While an island's light is dark, the first pointer move over its island
    // lights it. A light made with the pointer already over the island gets no
    // PointerEntered, and this is what lights it; and should the island ever
    // report the pointer gone while it is still there, the next move puts the
    // light back. A burning light does not listen to moves at all.
    void watch(island_spot_light& island_light, winrt::Microsoft::UI::Xaml::UIElement const& root) {
        island_light.moved =
            root.PointerMoved(winrt::auto_revoke, [this](auto const& sender, auto&&) {
                shine(sender.template as<winrt::Microsoft::UI::Xaml::UIElement>(), true);
            });
    }

    // Takes the layer out of its island's light; a light with nothing left to
    // light goes out for good.
    void leave(winrt::Microsoft::UI::Xaml::UIElement const& root,
               winrt::Microsoft::UI::Composition::Visual const& layer) {
        island_spot_light const* const island_light = island_light_of(root);
        if (!island_light) return;
        auto const targets = island_light->light.Targets();
        targets.Remove(layer);
        if (targets.Count() != 0) return;
        island_lights.erase(island_lights.begin() + (island_light - island_lights.data()));
    }

    // The cone that gives a spot of `size` from `height`, the inner cone
    // that gives the disc of `full`, and everything else the light takes as
    // it is. The cones are set by the height at rest and do not follow a
    // light that a press has lowered: it gives a spot smaller by as much.
    void aim(island_spot_light const& island_light) const {
        auto const tint = std::bit_cast<winrt::Windows::UI::Color>(color);
        float const outer = std::atan2(size / 2.0f, height);
        island_light.light.InnerConeColor(tint);
        island_light.light.OuterConeColor(tint);
        island_light.light.InnerConeAngle(std::min(std::atan2(full / 2.0f, height), outer));
        island_light.light.OuterConeAngle(outer);
        island_light.light.ConstantAttenuation(constant_attenuation);
        island_light.light.LinearAttenuation(linear_attenuation);
    }

    // Over the pointer, at the height in the light's own properties: the
    // compositor works the place out itself from the island's pointer
    // position.
    void raise(island_spot_light const& island_light) const {
        auto const over = island_light.light.Compositor().CreateExpressionAnimation(
            L"pointer.Position + Vector3(0, 0, light.Height)");
        over.SetReferenceParameter(L"pointer", island_light.pointer);
        over.SetReferenceParameter(L"light", island_light.light.Properties());
        island_light.light.StartAnimation(L"Offset", over);
    }

    // The light's intensity goes to where it belongs from wherever it is: one
    // key frame, at the end.
    void fade(island_spot_light const& island_light) const {
        auto const to = island_light.light.Compositor().CreateScalarKeyFrameAnimation();
        to.InsertKeyFrame(1.0f, island_light.shining ? intensity : 0.0f);
        to.Duration(fade_time);
        island_light.light.StartAnimation(L"InnerConeIntensity", to);
        island_light.light.StartAnimation(L"OuterConeIntensity", to);
    }

    void shine(winrt::Microsoft::UI::Xaml::UIElement const& root, bool on) {
        island_spot_light* const island_light = island_light_of(root);
        if (!island_light || island_light->shining == on) return;
        island_light->shining = on;
        fade(*island_light);
        // Last, and nothing after it: called from the move handler, the
        // revocation takes that handler away.
        if (on) {
            island_light->moved.revoke();
        } else {
            watch(*island_light, root);
        }
    }

    void flash(winrt::Microsoft::UI::Xaml::UIElement const& root) {
        island_spot_light const* const island_light = island_light_of(root);
        if (!island_light || !island_light->shining) return;
        auto const pulse = island_light->light.Compositor().CreateScalarKeyFrameAnimation();
        pulse.InsertKeyFrame(0.0f, intensity * flash_gain);
        pulse.InsertKeyFrame(1.0f, intensity);
        pulse.Duration(flash_time);
        island_light->light.StartAnimation(L"InnerConeIntensity", pulse);
        island_light->light.StartAnimation(L"OuterConeIntensity", pulse);
    }

    void retune() const {
        for (island_spot_light const& island_light : island_lights) aim(island_light);
    }

    void set_size(double value) {
        size = static_cast<float>(std::max(value, 0.0));
        retune();
    }

    void set_full(double value) {
        full = static_cast<float>(std::max(value, 0.0));
        retune();
    }

    void set_height(double value) {
        height = static_cast<float>(std::max(value, 0.0));
        retune();
        for (island_spot_light const& island_light : island_lights) {
            island_light.light.Properties().InsertScalar(height_property, height);
        }
    }

    void set_intensity(double value) {
        intensity = static_cast<float>(std::max(value, 0.0));
        for (island_spot_light const& island_light : island_lights) fade(island_light);
    }

    void set_color(Color value) {
        color = value;
        retune();
    }

    void set_constant_attenuation(double value) {
        constant_attenuation = static_cast<float>(value);
        retune();
    }

    void set_linear_attenuation(double value) {
        linear_attenuation = static_cast<float>(value);
        retune();
    }
};

}  // namespace wxl::impl
