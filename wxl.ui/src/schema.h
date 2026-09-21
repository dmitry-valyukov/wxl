#pragma once

// The whole schema: what the generator writes for the classes it generates,
// and beside it the classes written by hand that take tags in their braces.
// Include this one, never generated/schema.h directly -- see generated/schema.h
// for what a schema anchor is for.
//
// Every entry here comes with its compile-only line in
// test/dsl_surface.cpp: no generator writes one for it.

#include "BevelEffect.h"
#include "MagnifyEffect.h"
#include "generated/schema.h"

namespace wxl::dsl::schema {

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

}  // namespace wxl::dsl::schema
