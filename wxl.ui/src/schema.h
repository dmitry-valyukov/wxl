#pragma once

// The whole schema: what the generator writes for the classes it generates,
// and beside it the classes written by hand that take tags in their braces.
// Include this one, never wxl/schema.h directly.
//
// It is the same vocabulary as the generated Members.h, reached through the
// class that declares it: `schema::Button::content` beside the bare
// `dsl::content`. For finding a name rather than remembering it --
// `schema::Button::` offers exactly what a Button takes -- and for two things
// the flat form cannot carry: the anchor knows the class it was named
// through, so writing one class's member on another is refused by name, and
// it knows the type *that* class declares the property with, so the braced
// form survives where two classes disagree.
//
// Each struct mirrors its class's own base, and declares only the members
// that class declares itself; everything else arrives by inheritance, exactly
// as it does on the wrapper.
//
// Every entry here comes with its compile-only line in
// test/dsl_surface.cpp: no generator writes one for it.

#include "BevelEffect.h"
#include "Button3DEffect.h"
#include "GaussianBlurEffect.h"
#include "GlassEffect.h"
#include "HaloEffect.h"
#include "MagnifyEffect.h"
#include "RevealEffect.h"

#include "SettingsCard.h"
#include "SettingsExpander.h"
#include "HeaderedContentControl.h"
#include "AvailableSizeLayout.h"
#include <wxl/schema.h>

namespace wxl::dsl::schema {

struct HaloEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Color, ::wxl::Color, ::wxl::HaloEffect> color{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::BlurRadius, double, ::wxl::HaloEffect> blurRadius{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Opacity, double, ::wxl::HaloEffect> opacity{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Offset, ::wxl::Vector3, ::wxl::HaloEffect> offset{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::ZIndex, int32_t, ::wxl::HaloEffect> zIndex{};
};

struct GaussianBlurEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Color, ::wxl::Color, ::wxl::GaussianBlurEffect> color{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::BlurRadius, double, ::wxl::GaussianBlurEffect> blurRadius{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Opacity, double, ::wxl::GaussianBlurEffect> opacity{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Gamma, double, ::wxl::GaussianBlurEffect> gamma{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::ZIndex, int32_t, ::wxl::GaussianBlurEffect> zIndex{};
};

struct GlassEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Color, ::wxl::Color, ::wxl::GlassEffect> color{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::BlurRadius, double, ::wxl::GlassEffect> blurRadius{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Opacity, double, ::wxl::GlassEffect> opacity{};
};

struct Button3DEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Foreground, ::wxl::Color, ::wxl::Button3DEffect> foreground{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Background, ::wxl::Color, ::wxl::Button3DEffect> background{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Shadow, double, ::wxl::Button3DEffect> shadow{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Emboss, double, ::wxl::Button3DEffect> emboss{};
};

struct BevelEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::StrokeThickness, double, ::wxl::BevelEffect> strokeThickness{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::BlurRadius, double, ::wxl::BevelEffect> blurRadius{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Offset, double, ::wxl::BevelEffect> offset{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Margin, ::wxl::Thickness, ::wxl::BevelEffect> margin{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::CornerRadius, ::wxl::CornerRadius, ::wxl::BevelEffect> cornerRadius{};
};

struct MagnifyEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Scale, ::wxl::Size, ::wxl::MagnifyEffect> scale{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Maximum, double, ::wxl::MagnifyEffect> maximum{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Minimum, double, ::wxl::MagnifyEffect> minimum{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Duration, ::wxl::core::duration, ::wxl::MagnifyEffect> duration{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::DelayTime, ::wxl::core::duration, ::wxl::MagnifyEffect> delayTime{};
};

struct HoverLight {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Size, double, ::wxl::HoverLight> size{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Height, double, ::wxl::HoverLight> height{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Intensity, double, ::wxl::HoverLight> intensity{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::DiffuseAmount, double, ::wxl::HoverLight> diffuseAmount{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Color, ::wxl::Color, ::wxl::HoverLight> color{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::ConstantAttenuation, double, ::wxl::HoverLight> constantAttenuation{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::LinearAttenuation, double, ::wxl::HoverLight> linearAttenuation{};
};

struct BorderLight {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Size, double, ::wxl::BorderLight> size{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Height, double, ::wxl::BorderLight> height{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Intensity, double, ::wxl::BorderLight> intensity{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::DiffuseAmount, double, ::wxl::BorderLight> diffuseAmount{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Color, ::wxl::Color, ::wxl::BorderLight> color{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::ConstantAttenuation, double, ::wxl::BorderLight> constantAttenuation{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::LinearAttenuation, double, ::wxl::BorderLight> linearAttenuation{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::StrokeThickness, double, ::wxl::BorderLight> strokeThickness{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::CornerRadius, ::wxl::CornerRadius, ::wxl::BorderLight> cornerRadius{};
};

struct RevealEffect {
    static constexpr ::wxl::Property<::wxl::PropertyKey::ZIndex, int32_t, ::wxl::RevealEffect> zIndex{};
};


struct SettingsCard : ::wxl::dsl::schema::ContentControl {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Header, ::wxl::Object, ::wxl::SettingsCard> header{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Description, ::wxl::hstring, ::wxl::SettingsCard> description{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::HeaderIcon, ::wxl::IconElement, ::wxl::SettingsCard> headerIcon{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::ActionIcon, ::wxl::IconElement, ::wxl::SettingsCard> actionIcon{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::IsClickEnabled, bool, ::wxl::SettingsCard> isClickEnabled{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::IsActionIconVisible, bool, ::wxl::SettingsCard> isActionIconVisible{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::ContentAlignment, ::wxl::SettingsCardContentAlignment, ::wxl::SettingsCard> contentAlignment{};
};

struct SettingsExpander : ::wxl::dsl::schema::ContentControl {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Header, ::wxl::Object, ::wxl::SettingsExpander> header{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::Description, ::wxl::hstring, ::wxl::SettingsExpander> description{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::HeaderIcon, ::wxl::IconElement, ::wxl::SettingsExpander> headerIcon{};
    static constexpr ::wxl::CollectionProperty<::wxl::PropertyKey::Items, ::wxl::SettingsExpander> items{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::IsExpanded, bool, ::wxl::SettingsExpander> isExpanded{};
};
struct HeaderedContentControl : ::wxl::dsl::schema::ContentControl {
    static constexpr ::wxl::Property<::wxl::PropertyKey::Header, ::wxl::Object, ::wxl::HeaderedContentControl> header{};
};
struct AvailableSizeLayout {
    static constexpr ::wxl::Property<::wxl::PropertyKey::AvailableWidth, double, ::wxl::AvailableSizeLayout> availableWidth{};
    static constexpr ::wxl::Property<::wxl::PropertyKey::AvailableHeight, double, ::wxl::AvailableSizeLayout> availableHeight{};
};
}  // namespace wxl::dsl::schema
