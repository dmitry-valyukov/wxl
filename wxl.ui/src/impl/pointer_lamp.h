#pragma once

// A spotlight over the pointer, one per XAML island, as RevealEffect's two
// lights and Button3DEffect's share it: the settings of one lamp and the
// lamps burning with them -- one for each island the light has an element
// in. The lamp follows the pointer by an expression the compositor
// evaluates, comes up when the pointer enters the island and goes out when
// it leaves, and is put out by its intensity, never by IsEnabled: a sprite
// wearing a lighting brush that no enabled light is aimed at is drawn white
// all over.
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

// How long a lamp takes to come up and to go out, and what a press does to
// the spot: three times the light at once, back to its own over the flash.
constexpr std::chrono::milliseconds lampFadeTime{180};
constexpr std::chrono::milliseconds lampFlashTime{400};
constexpr float lampFlashGain = 3.0f;

// The height the lamp stands at, in its own property set: the expression
// that places it reads it from there, so a press can move the lamp by
// animating one scalar.
constexpr wchar_t const* lampHeightProperty = L"Height";

// One lamp: the spotlight over the pointer of one island, shining straight
// down. The island is named by the element at its root, held weakly. The
// lamp listens to that root for the pointer coming and going, and stops
// listening when it goes itself: the root holds nothing of the light.
struct Lamp {
    winrt::weak_ref<winrt::Microsoft::UI::Xaml::UIElement> root;
    winrt::Microsoft::UI::Composition::CompositionPropertySet pointer{nullptr};
    winrt::Microsoft::UI::Composition::SpotLight light{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement::PointerEntered_revoker entered;
    winrt::Microsoft::UI::Xaml::UIElement::PointerExited_revoker exited;
    winrt::Microsoft::UI::Xaml::UIElement::PointerMoved_revoker moved;
    // The pointer is over the island.
    bool shining = false;
};

// The settings of one spotlight and the lamps that burn with them.
struct Beam : core::sta_refcounted {
    float size;
    // The disc of full light inside the spot; 0 for a spot that fades from
    // its very axis.
    float full = 0.0f;
    float height = 120.0f;
    float intensity = 1.0f;
    float diffuse;
    Color color = rgb(255, 255, 255);
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.0f;
    std::vector<Lamp> lamps;

    Beam(float size, float diffuse) : size{size}, diffuse{diffuse} {}

    Lamp* lamp_of(winrt::Microsoft::UI::Xaml::UIElement const& root) {
        for (Lamp& lamp : lamps) {
            if (lamp.root.get() == root) return &lamp;
        }
        return nullptr;
    }

    // Aims the lamp of the layer's island at the layer, lighting one up over
    // that island if this is its first.
    void light_up(winrt::Microsoft::UI::Xaml::UIElement const& root,
                  winrt::Microsoft::UI::Composition::Visual const& layer) {
        // An island that is gone takes its lamp with it.
        std::erase_if(lamps, [](Lamp const& lamp) { return !lamp.root.get(); });
        if (Lamp const* const known = lamp_of(root)) {
            known->light.Targets().Add(layer);
            return;
        }

        auto const space = winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(root);

        Lamp lamp;
        lamp.root = root;
        lamp.pointer = winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetPointerPositionPropertySet(root);
        lamp.light = space.Compositor().CreateSpotLight();
        lamp.light.CoordinateSpace(space);
        lamp.light.Direction({0.0f, 0.0f, -1.0f});
        lamp.light.InnerConeIntensity(0.0f);
        lamp.light.OuterConeIntensity(0.0f);
        lamp.light.Properties().InsertScalar(lampHeightProperty, height);
        lamp.light.Targets().Add(layer);
        aim(lamp);
        raise(lamp);

        // The handlers hold the light by a bare pointer: they are revoked
        // with the lamp, and the lamp goes no later than the light does.
        lamp.entered = root.PointerEntered(winrt::auto_revoke, [this](auto const& sender, auto&&) {
            shine(sender.template as<winrt::Microsoft::UI::Xaml::UIElement>(), true);
        });
        lamp.exited = root.PointerExited(winrt::auto_revoke, [this](auto const& sender, auto&&) {
            shine(sender.template as<winrt::Microsoft::UI::Xaml::UIElement>(), false);
        });
        watch(lamp, root);

        lamps.push_back(std::move(lamp));
    }

    // While a lamp is dark, the first pointer move over its island lights it.
    // A lamp made with the pointer already over the island gets no
    // PointerEntered, and this is what lights it; and should the island ever
    // report the pointer gone while it is still there, the next move puts the
    // light back. A burning lamp does not listen to moves at all.
    void watch(Lamp& lamp, winrt::Microsoft::UI::Xaml::UIElement const& root) {
        lamp.moved = root.PointerMoved(winrt::auto_revoke, [this](auto const& sender, auto&&) {
            shine(sender.template as<winrt::Microsoft::UI::Xaml::UIElement>(), true);
        });
    }

    // Takes the layer out of its lamp's light; a lamp with nothing left to
    // light goes out for good.
    void leave(winrt::Microsoft::UI::Xaml::UIElement const& root,
               winrt::Microsoft::UI::Composition::Visual const& layer) {
        Lamp const* const lamp = lamp_of(root);
        if (!lamp) return;
        auto const targets = lamp->light.Targets();
        targets.Remove(layer);
        if (targets.Count() != 0) return;
        lamps.erase(lamps.begin() + (lamp - lamps.data()));
    }

    // The cone that gives a spot of `size` from `height`, the inner cone
    // that gives the disc of `full`, and everything else the lamp takes as
    // it is. The cones are set by the height at rest and do not follow a
    // lamp that a press has lowered: it gives a spot smaller by as much.
    void aim(Lamp const& lamp) const {
        auto const tint = std::bit_cast<winrt::Windows::UI::Color>(color);
        float const outer = std::atan2(size / 2.0f, height);
        lamp.light.InnerConeColor(tint);
        lamp.light.OuterConeColor(tint);
        lamp.light.InnerConeAngle(std::min(std::atan2(full / 2.0f, height), outer));
        lamp.light.OuterConeAngle(outer);
        lamp.light.ConstantAttenuation(constantAttenuation);
        lamp.light.LinearAttenuation(linearAttenuation);
    }

    // Over the pointer, at the height in the lamp's own properties: the
    // compositor works the place out itself from the island's pointer
    // position.
    void raise(Lamp const& lamp) const {
        auto const over =
            lamp.light.Compositor().CreateExpressionAnimation(L"pointer.Position + Vector3(0, 0, lamp.Height)");
        over.SetReferenceParameter(L"pointer", lamp.pointer);
        over.SetReferenceParameter(L"lamp", lamp.light.Properties());
        lamp.light.StartAnimation(L"Offset", over);
    }

    // The lamp's intensity goes to where it belongs from wherever it is: one
    // key frame, at the end.
    void fade(Lamp const& lamp) const {
        auto const to = lamp.light.Compositor().CreateScalarKeyFrameAnimation();
        to.InsertKeyFrame(1.0f, lamp.shining ? intensity : 0.0f);
        to.Duration(lampFadeTime);
        lamp.light.StartAnimation(L"InnerConeIntensity", to);
        lamp.light.StartAnimation(L"OuterConeIntensity", to);
    }

    void shine(winrt::Microsoft::UI::Xaml::UIElement const& root, bool on) {
        Lamp* const lamp = lamp_of(root);
        if (!lamp || lamp->shining == on) return;
        lamp->shining = on;
        fade(*lamp);
        // Last, and nothing after it: called from the move handler, the
        // revocation takes that handler away.
        if (on) {
            lamp->moved.revoke();
        } else {
            watch(*lamp, root);
        }
    }

    void flash(winrt::Microsoft::UI::Xaml::UIElement const& root) {
        Lamp const* const lamp = lamp_of(root);
        if (!lamp || !lamp->shining) return;
        auto const pulse = lamp->light.Compositor().CreateScalarKeyFrameAnimation();
        pulse.InsertKeyFrame(0.0f, intensity * lampFlashGain);
        pulse.InsertKeyFrame(1.0f, intensity);
        pulse.Duration(lampFlashTime);
        lamp->light.StartAnimation(L"InnerConeIntensity", pulse);
        lamp->light.StartAnimation(L"OuterConeIntensity", pulse);
    }

    void retune() const {
        for (Lamp const& lamp : lamps) aim(lamp);
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
        for (Lamp const& lamp : lamps) lamp.light.Properties().InsertScalar(lampHeightProperty, height);
    }

    void set_intensity(double value) {
        intensity = static_cast<float>(std::max(value, 0.0));
        for (Lamp const& lamp : lamps) fade(lamp);
    }

    void set_color(Color value) {
        color = value;
        retune();
    }

    void set_constant_attenuation(double value) {
        constantAttenuation = static_cast<float>(value);
        retune();
    }

    void set_linear_attenuation(double value) {
        linearAttenuation = static_cast<float>(value);
        retune();
    }
};

}  // namespace wxl::impl
