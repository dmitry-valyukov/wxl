#pragma once

// wxl::Card -- the card look, as a control: a rounded border wearing the
// framework's own card brushes, lifted off the page far enough for its
// ThemeShadow to be seen.
//
// Written here rather than in every application because that is what it is.
// Material 3 hands a Card out of its library and nobody rewrites it per app;
// WinUI has the resources -- CardBackgroundFillColorDefault,
// CardStrokeColorDefault -- but no control that wears them, so each
// application had been describing the same six lines: the radius, the two
// brushes, the hairline, the shadow, the z that makes the shadow visible,
// and the padding.
//
//     auto card = Card {
//         hAlign.right, Margin {0, 64, 72, 0},
//         Rows { title, buttons },
//     };
//
// A Border, and written as one. The look goes on in the constructor, ahead of
// the caller's setters, so everything here is a default in the honest sense:
// a card that needs a translucent face over artwork writes `background = ...`
// and gets it.
//
// A class rather than a Template<Border> because a card is always built and
// never worn -- nobody dresses an existing Border as a card -- and the template
// form would charge every use the extra word `Template<Border>`. Presets keep
// the cases a class cannot serve: a look an application keeps to itself, one
// that has to go on an object somebody else built, or one nested inside
// another. Rows and Columns next door are the same shape for the same reason.
//
// The look and nothing else. Where the card stands -- alignment, margins,
// which corner of which screen -- belongs to the page that places it, and a
// card carrying placement would have to be argued with at every use.
//
// The radius is the framework's own for a raised surface (OverlayCornerRadius
// is 8), not the control radius: a card is a surface, not a button.
//
// The dsl tags are written qualified -- `dsl::background` rather than
// `background` -- because inside a Border subclass the bare name finds the
// base's accessor and never reaches the tag.
//
// ThemeShadow is constructed live here and that is safe, unlike in a
// namespace-scope preset: this constructor runs when the card is built, long
// after wxl has brought the runtime up.

#include "Color.h"
#include "CornerRadius.h"
#include "Thickness.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/brushes.h"

namespace wxl {

class Card : public Border {
public:
    template <typename... Setters>
        requires impl::setter_pack<Card, Setters...>
    explicit Card(Setters&&... setters)
        : Border{
              CornerRadius{12},
              dsl::background = dsl::brushes.Card.BackgroundFillColor.Default,
              dsl::borderBrush = dsl::brushes.Card.StrokeColorDefault,
              BorderThickness{1},
              dsl::shadow = ThemeShadow{},
              dsl::translation = {0.0f, 0.0f, 32.0f},
              Padding{16},
              std::forward<Setters>(setters)...,
          } {}

protected:
    // The door every generated wrapper has, and a hand-written class has to
    // say. Two things go through it, and both are how wxl gets a control that
    // it did not just build:
    //
    //   * try_as, which builds a wrapper as T{new T::Impl{...}} -- so without
    //     this a handler could not name Card as its sender, and reading the
    //     sender is the only legitimate way to reach a control of the
    //     description tree;
    //   * core::nullable<T>, whose sentinel builds the empty state through a
    //     maker derived from the wrapper, which hands its own base a null
    //     Impl -- so without this a screen could keep a core::nullable<Border>
    //     but not a core::nullable<Card>, and the look would be spendable but
    //     unstorable.
    explicit Card(Impl* impl) noexcept : Border{impl} {}

    friend class Object::Impl;
};

// wxl::OverlayCard -- the same card for a page that has a picture under it.
//
// The framework's card resources are meant for a card on a page, and a page
// is a flat colour: over artwork they read as a bright plate stamped onto the
// picture, and in-app acrylic does no better in the light theme -- it tints
// what it blurs, and a bright picture tints it bright. What works over an
// image is the other way round: a dark scrim the picture shows through, with
// a light hairline to give the edge back.
//
// The scrim is neutral on purpose. A page whose picture has a colour of its
// own tints it in one line -- `background = SolidColorBrush{ARGB{...}}` --
// the way the book reader warms it to the brown of its own cover art.
//
// The ink on it is the dark theme's ink, whatever theme the window wears:
// the scrim is dark in both, so `foreground = brushes.Text.FillColor.Primary`
// -- near-black in the light theme -- writes the text out of existence. The
// call form of a brush names the theme to read it from, which is what it is
// for: `brushes.Text.FillColor.Primary(ElementTheme::Dark)`. A control that
// paints itself from the accent colour, ProgressRing among them, needs the
// same treatment for the same reason.
class OverlayCard : public Border {
public:
    template <typename... Setters>
        requires impl::setter_pack<OverlayCard, Setters...>
    explicit OverlayCard(Setters&&... setters)
        : Border{
              CornerRadius{12},
              dsl::background = SolidColorBrush{ARGB{0x6C1A1A1A}},
              dsl::borderBrush = SolidColorBrush{ARGB{0x33FFFFFF}},
              BorderThickness{1},
              dsl::shadow = ThemeShadow{},
              dsl::translation = {0.0f, 0.0f, 32.0f},
              Padding{16},
              std::forward<Setters>(setters)...,
          } {}

protected:
    explicit OverlayCard(Impl* impl) noexcept : Border{impl} {}

    friend class Object::Impl;
};

// Their bodies live in Card.cpp: a handler may name either as its sender,
// and only there is try_as defined.
extern template Card Object::try_as<Card>() const;
extern template OverlayCard Object::try_as<OverlayCard>() const;

}  // namespace wxl
