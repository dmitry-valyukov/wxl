// Generated once from Controls/DesignGuidance/ColorSections of the original by the script of the port; edited by hand since.
// The six sections of the Color page: the brushes of the framework, a tile of each, with what it is for.

#include "Pages.h"

#include "ColorTile.h"

using namespace wxl;
using namespace wxl::dsl;

namespace gallery {

FrameworkElement textSection() {
    return StackPanel {
        spacing = 4.0,
        colorExample(u"Text", u"For UI labels and static text.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Text")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.Text.FillColor.Primary, brushes.Text.OnAccent.FillColor.Primary, {u"Text / Primary", u"Rest or Hover", u"TextFillColorPrimaryBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Text.FillColor.Secondary, brushes.Text.OnAccent.FillColor.Primary, {u"Text / Secondary", u"Rest or Hover", u"TextFillColorSecondaryBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Text.FillColor.Tertiary, brushes.Text.OnAccent.FillColor.Primary, {u"Text / Tertiary", u"Pressed only (not accessible)", u"TextFillColorTertiaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.Text.FillColor.Disabled, brushes.Text.FillColor.Primary, {u"Text / Disabled", u"Disabled only (not accessible)", u"TextFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        colorExample(u"Accent Text", u"Recommended for links.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Accent Text")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.Accent.TextFillColor.Primary, brushes.Text.OnAccent.FillColor.Primary, {u"Accent Text / Primary", u"Rest or Hover", u"AccentTextFillColorPrimaryBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Accent.TextFillColor.Secondary, brushes.Text.OnAccent.FillColor.Primary, {u"Accent Text / Secondary", u"Rest or Hover", u"AccentTextFillColorSecondaryBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Accent.TextFillColor.Tertiary, brushes.Text.OnAccent.FillColor.Primary, {u"Accent Text / Tertiary", u"Pressed only (not accessible)", u"AccentTextFillColorTertiaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.Accent.TextFillColor.Disabled, brushes.Text.FillColor.Primary, {u"Accent Text / Disabled", u"Disabled only (not accessible)", u"AccentTextFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        colorExample(u"Text On Accent", u"Used for text on accent colored controls or fills.", brushes.Accent.FillColor.Default, brushes.Text.OnAccent.FillColor.Primary, colorSample(u"Text On Accent")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Text.OnAccent.FillColor.Primary, brushes.Text.FillColor.Primary, {u"Text on Accent / Primary", u"Rest or Hover", u"TextOnAccentFillColorPrimaryBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Text.OnAccent.FillColor.Secondary, brushes.Text.FillColor.Primary, {u"Text on Accent / Secondary", u"Pressed only (not accessible)", u"TextOnAccentFillColorSecondaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Text.OnAccent.FillColor.Disabled, rgb(0, 0, 0), {u"Text on Accent / Disabled", u"Disabled only (not accessible)", u"TextOnAccentFillColorDisabledBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Text.OnAccent.FillColor.SelectedText, rgb(0, 0, 0), {u"Text on Accent / Selected Text", u"For highlighted text in text entry experiences", u"TextOnAccentFillColorSelectedTextBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
    };
}

FrameworkElement fillSection() {
    return StackPanel {
        spacing = 4.0,
        colorExample(u"Control Fill", u"Fill used for standard controls.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Control Fill")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.Control.FillColor.Default, nullptr, {u"Control / Default", u"Rest", u"ControlFillColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.FillColor.Secondary, nullptr, {u"Control / Secondary", u"Hover", u"ControlFillColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Control.FillColor.Tertiary, nullptr, {u"Control / Tertiary", u"Pressed", u"ControlFillColorTertiaryBrush", true, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.Control.FillColor.Quarternary, nullptr, {u"Control / Quartenary", u"Rest (Pill Button control)", u"ControlFillColorQuarternaryBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Control.FillColor.Disabled, nullptr, {u"Control / Disabled", u"Disabled", u"ControlFillColorDisabledBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.FillColor.Transparent, nullptr, {u"Control / Transparent", u"Rest", u"ControlFillColorTransparentBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Control.FillColor.InputActive, nullptr, {u"Control / Input Active", u"Active/focused text input fields", u"ControlFillColorInputActiveBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Control Alt Fill", u"Fill used for the 'off' states of toggle controls.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Control Alt Fill")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Control.AltFillColor.Transparent, nullptr, {u"Control Alt / Transparent", u"", u"ControlAltFillColorTransparentBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.AltFillColor.Secondary, nullptr, {u"Control Alt / Secondary", u"Rest", u"ControlAltFillColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Control.AltFillColor.Tertiary, nullptr, {u"Control Alt / Tertiary", u"Hover", u"ControlAltFillColorTertiaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Control.AltFillColor.Quarternary, nullptr, {u"Control Alt / Quarternary", u"Pressed", u"ControlAltFillColorQuarternaryBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.AltFillColor.Disabled, nullptr, {u"Control Alt / Disabled", u"Disabled", u"ControlAltFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
        colorExample(u"Neutral Solid", u"Fills used for Sliders thumb control to cover the track beneath it.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Neutral Solid")),
        gallery::tileGrid(1, 1, {
            gallery::colorTile(brushes.Control.SolidFillColorDefault, nullptr, {u"Control Solid / Default", u"Rest", u"ControlSolidFillColorDefaultBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
        }),
        colorExample(u"Neutral Strong", u"Used for controls that must meet contrast ratio requirements of 3:1.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Neutral Strong")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Control.Strong.FillColorDefault, brushes.Text.FillColor.Inverse, {u"Control Strong / Default", u"Rest or hover", u"ControlStrongFillColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.Strong.FillColorDisabled, nullptr, {u"Control Strong / Disabled", u"Disabled only (not accessible)", u"ControlStrongFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Subtle Fill", u"Used for list items and fills that are transparent at rest and appear upon interaction.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Subtle Fill")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.SubtleFillColor.Transparent, nullptr, {u"Subtle / Transparent", u"Rest", u"SubtleFillColorTransparentBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SubtleFillColor.Secondary, nullptr, {u"Subtle / Secondary", u"Hover", u"SubtleFillColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SubtleFillColor.Tertiary, nullptr, {u"Subtle / Tertiary", u"Pressed", u"SubtleFillColorTertiaryBrush", true, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.SubtleFillColor.Disabled, nullptr, {u"Subtle / Disabled", u"Disabled only (not accessible)", u"SubtleFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        colorExample(u"Control On Image Fill", u"Used for controls living on top of imagery.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Control On Image Fill")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.Control.OnImageFillColor.Default, nullptr, {u"Control On Image Fill Default", u"Rest", u"ControlOnImageFillColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.OnImageFillColor.Secondary, nullptr, {u"Control On Image Fill Secondary", u"Hover", u"ControlOnImageFillColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Control.OnImageFillColor.Tertiary, nullptr, {u"Control On Image Fill Tertiary", u"Pressed", u"ControlOnImageFillColorTertiaryBrush", true, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.Control.OnImageFillColor.Disabled, nullptr, {u"Control On Image Fill Disabled", u"Disabled only (not accessible)", u"ControlOnImageFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        colorExample(u"Accent Fill", u"Used for accent fills on controls.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Accent Fill")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Accent.FillColor.Default, brushes.Text.OnAccent.FillColor.Default, {u"Accent / Default", u"Rest", u"AccentFillColorDefaultBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Accent.FillColor.Secondary, brushes.Text.OnAccent.FillColor.Default, {u"Accent / Secondary", u"Hover", u"AccentFillColorSecondaryBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Accent.FillColor.Tertiary, brushes.Text.OnAccent.FillColor.Default, {u"Accent / Tertiary", u"Pressed", u"AccentFillColorTertiaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Accent.FillColor.Disabled, nullptr, {u"Accent / Disabled", u"Disabled", u"AccentFillColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Accent.FillColor.SelectedTextBackground, brushes.Text.OnAccent.FillColor.Default, {u"Accent / Selected Text Background", u"Highighted/selected text background", u"AccentFillColorSelectedTextBackgroundBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
    };
}

FrameworkElement strokeSection() {
    return StackPanel {
        spacing = 4.0,
        colorExample(u"Card Stroke", u"Used for card and layer colors.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Card Stroke")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Card.StrokeColorDefault, nullptr, {u"Card Stroke / Default", u"Card layer and strokes", u"CardStrokeColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Card.StrokeColorDefaultSolid, nullptr, {u"Card Stroke / Default Solid", u"Solid equivalent of Card Stroke / Default. Used in command bar for expanded states", u"CardStrokeColorDefaultSolidBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
        colorExample(u"Control Elevation (gradient strokes)", u"Used for standard control strokes and stroke states.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Control Elevation (gradient strokes)")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Control.ElevationBorder, nullptr, {u"Control / Border", u"Rest", u"ControlElevationBorderBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.CircleElevationBorder, nullptr, {u"Circle / Border", u"Rest", u"CircleElevationBorderBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Text.Control.ElevationBorder, nullptr, {u"Text Control / Border", u"Rest", u"TextControlElevationBorderBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Text.Control.ElevationBorderFocused, nullptr, {u"Text Control / Border Focused", u"Active text fields", u"TextControlElevationBorderFocusedBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Accent.ControlElevationBorder, nullptr, {u"Accent Control / Border", u"Rest", u"AccentControlElevationBorderBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
        colorExample(u"Control Stroke", u"Used for gradient stops in elevation borders, and for control states.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Control Stroke")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.Control.StrokeColor.Default, nullptr, {u"Control Stroke / Default", u"Used in Control Elevation Brushes. Pressed or Disabled", u"ControlStrokeColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.StrokeColor.Secondary, nullptr, {u"Control Stroke / Secondary", u"Used in Control Elevation Brushes", u"ControlStrokeColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Control.StrokeColor.OnAccent.Default, nullptr, {u"Control Stroke / On Accent Default", u"Used in Control Elevation Brushes. Pressed or Disabled", u"ControlStrokeColorOnAccentDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.Control.StrokeColor.OnAccent.Secondary, nullptr, {u"Control Stroke / On Accent Secondary", u"Used in Control Elevation Brushes", u"ControlStrokeColorOnAccentSecondaryBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Control.StrokeColor.OnAccent.Tertiary, nullptr, {u"Control Stroke / On Accent Tertiary", u"Linework on Accent controls, ie: dividers", u"ControlStrokeColorOnAccentTertiaryBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.StrokeColor.OnAccent.Disabled, nullptr, {u"Control Stroke / On Accent Disabled", u"Disabled", u"ControlStrokeColorOnAccentDisabledBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Control.StrokeColor.ForStrongFillWhenOnImage, nullptr, {u"Control Stroke / For Strong Fill When On Image", u"When used with a 'strong' fill color, ensures a 3:1 contrast on any background", u"ControlStrokeColorForStrongFillWhenOnImageBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Control Strong Stroke", u"Used for control strokes that must meet contrast ratio requirements of 3:1.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Control Strong Stroke")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Control.Strong.StrokeColorDefault, brushes.Text.FillColor.Inverse, {u"Control Strong Stroke / Default", u"3:1 control border", u"ControlStrongStrokeColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Control.Strong.StrokeColorDisabled, nullptr, {u"Control Strong Stroke / Disabled", u"Disabled", u"ControlStrongStrokeColorDisabledBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Surface Stroke", u"Used for strokes on background surfaces, ie: flyouts, windows, dialogs.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Surface Stroke")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SurfaceStrokeColor.Default, nullptr, {u"Surface Stroke / Default", u"Window and dialog borders, theme inverse", u"SurfaceStrokeColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SurfaceStrokeColor.Flyout, nullptr, {u"Surface Stroke / Flyout", u"Control flyouts, always dark", u"SurfaceStrokeColorFlyoutBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Divider Stroke", u"Used for divider and graphic lines.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Divider Stroke")),
        gallery::tileGrid(1, 1, {
            gallery::colorTile(brushes.DividerStrokeColorDefault, nullptr, {u"Divider Stroke / Default", u"Content dividers", u"DividerStrokeColorDefaultBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
        }),
        colorExample(u"Focus Stroke", u"Used for divider and graphic lines. Theme inverse; dark in light theme and light in dark theme.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Focus Stroke")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.FocusStrokeColorOuter, brushes.Text.FillColor.Inverse, {u"Focus / Outer", u"Outer stroke color", u"FocusStrokeColorOuterBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.FocusStrokeColorInner, nullptr, {u"Focus / Inner", u"Inner stroke color", u"FocusStrokeColorInnerBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
    };
}

FrameworkElement backgroundSection() {
    return StackPanel {
        spacing = 4.0,
        colorExample(u"Card Background", u"Used to create 'cards' - content blocks that live on page and layer backgrounds.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Card Background")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.Card.BackgroundFillColor.Default, nullptr, {u"Card Background / Default", u"Default card color", u"CardBackgroundFillColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Card.BackgroundFillColor.Secondary, nullptr, {u"Card Background / Secondary", u"Alternate card color: slightly darker", u"CardBackgroundFillColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.Card.BackgroundFillColor.Tertiary, nullptr, {u"Card Background / Tertiary", u"Default card hover and pressed color", u"CardBackgroundFillColorTertiaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Smoke Background", u"Used over windows and desktop to block them out as inaccessible.", brushes.SmokeFillColorDefault, nullptr, colorSample(u"Smoke Background")),
        gallery::tileGrid(1, 1, {
            gallery::colorTile(brushes.SmokeFillColorDefault, nullptr, {u"Smoke / Default", u"Dims the background behind dialogs", u"SmokeFillColorDefaultBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
        }),
        colorExample(u"Layer", u"Used on background colors of any material to create layering.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Layer")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Layer.FillColorDefault, nullptr, {u"Layer / Default", u"Content layer color", u"LayerFillColorDefaultBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Layer.FillColorAlt, nullptr, {u"Layer / Alt", u"Alternate content layer color", u"LayerFillColorAltBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
        colorExample(u"Layer on Acrylic", u"Used on background colors of any material to create layering.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Layer on Acrylic")),
        gallery::tileGrid(1, 1, {
            gallery::colorTile(brushes.Layer.OnAcrylicFillColorDefault, nullptr, {u"Layer On Acrylic / Default", u"Content layer color on acrylic surfaces", u"LayerOnAcrylicFillColorDefaultBrush", false, gallery::ColorBackdrop::Acrylic, false, 0, 0}),
        }),
        colorExample(u"Layer on Mica Base Alt", u"Used for fills on Tab control.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Layer on Mica Base Alt")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.LayerOnMicaBaseAltFillColor.Default, nullptr, {u"Layer On Mica Base Alt / Default", u"Active Tab Rest, Content layer", u"LayerOnMicaBaseAltFillColorDefaultBrush", true, gallery::ColorBackdrop::MicaAlt, false, 0, 0}),
            gallery::colorTile(brushes.LayerOnMicaBaseAltFillColor.Tertiary, nullptr, {u"Layer On Mica Base Alt / Tertiary", u"Active Tab Drag", u"LayerOnMicaBaseAltFillColorTertiaryBrush", false, gallery::ColorBackdrop::MicaAlt, false, 0, 1}),
        }),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.LayerOnMicaBaseAltFillColor.Transparent, nullptr, {u"Layer On Mica Base Alt / Transparent", u"Inactive Tab Rest", u"LayerOnMicaBaseAltFillColorTransparentBrush", true, gallery::ColorBackdrop::MicaAlt, false, 0, 0}),
            gallery::colorTile(brushes.LayerOnMicaBaseAltFillColor.Secondary, nullptr, {u"Layer On Mica Base Alt / Secondary", u"Inactive Tab Hover", u"LayerOnMicaBaseAltFillColorSecondaryBrush", false, gallery::ColorBackdrop::MicaAlt, false, 0, 1}),
        }),
        colorExample(u"Solid Background", u"Solid background colors to place layers, cards or controls on.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Solid Background")),
        gallery::tileGrid(4, 1, {
            gallery::colorTile(brushes.SolidBackgroundFillColor.Base, nullptr, {u"Solid Background / Base", u"Used for the bottom most layer of an experience", u"SolidBackgroundFillColorBaseBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SolidBackgroundFillColor.BaseAlt, nullptr, {u"Solid Background / Base Alt", u"Used for the bottom most layer of an experience", u"SolidBackgroundFillColorBaseAltBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SolidBackgroundFillColor.Secondary, nullptr, {u"Solid Background / Secondary", u"Alternate base color for those who need a darker background color", u"SolidBackgroundFillColorSecondaryBrush", true, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(brushes.SolidBackgroundFillColor.Tertiary, nullptr, {u"Solid Background / Tertiary", u"Content layer color", u"SolidBackgroundFillColorTertiaryBrush", false, gallery::ColorBackdrop::None, false, 0, 3}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SolidBackgroundFillColor.Quarternary, nullptr, {u"Solid Background / Quarternary", u"Alt content layer color", u"SolidBackgroundFillColorQuarternaryBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SolidBackgroundFillColor.Quinary, nullptr, {u"Solid Background / Quinary", u"Used for solid default card colors", u"SolidBackgroundFillColorQuinaryBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SolidBackgroundFillColor.Senary, nullptr, {u"Solid Background / Senary", u"Used for solid default card colors", u"SolidBackgroundFillColorSenaryBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        colorExample(u"Mica Background", u"Mica background colors to place layers, cards, or controls on.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Mica Background")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(nullptr, nullptr, {u"Mica Background / Base", u"Used for the bottom most layer of an experience", u"", true, gallery::ColorBackdrop::Mica, true, 0, 0}),
            gallery::colorTile(nullptr, nullptr, {u"Mica Background / Base Alt", u"Default tab band background color", u"", false, gallery::ColorBackdrop::MicaAlt, true, 0, 1}),
        }),
        colorExample(u"Acrylic Background", u"Acrylic background colors to place layers, cards, or controls on.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Acrylic Background")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Acrylic.BackgroundFillColor.Base, nullptr, {u"Acrylic Background / Base", u"Used for the bottom most layer of an acrylic surface only when the surface will use layers", u"AcrylicBackgroundFillColorBaseBrush", true, gallery::ColorBackdrop::Acrylic, false, 0, 0}),
            gallery::colorTile(brushes.Acrylic.BackgroundFillColor.Default, nullptr, {u"Acrylic Background / Default", u"Default acrylic recipe used for control flyouts and surfaces that live with in the context of an app", u"AcrylicBackgroundFillColorDefaultBrush", false, gallery::ColorBackdrop::Acrylic, false, 0, 1}),
        }),
        colorExample(u"Accent Acrylic Background", u"Acrylic background colors to place layers, cards, or controls on.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"Accent Acrylic Background")),
        gallery::tileGrid(2, 1, {
            gallery::colorTile(brushes.Accent.Acrylic.BackgroundFillColorBase, nullptr, {u"Accent Acrylic Background / Base", u"Used for the bottom most layer of an acrylic surface only when the surface will use layers", u"AccentAcrylicBackgroundFillColorBaseBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.Accent.Acrylic.BackgroundFillColorDefault, nullptr, {u"Accent Acrylic Background / Default", u"Default acrylic recipe used for control flyouts and surfaces that live with in the context of an app", u"AccentAcrylicBackgroundFillColorDefaultBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
        }),
    };
}

FrameworkElement signalSection() {
    return StackPanel {
        spacing = 4.0,
        colorExample(u"System", u"Used for accent fills on controls.", brushes.SolidBackgroundFillColor.Quarternary, nullptr, colorSample(u"System")),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SystemFillColor.Success, brushes.Text.FillColor.Inverse, {u"System / Success", u"Badge", u"SystemFillColorSuccessBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SystemFillColor.Caution, brushes.Text.FillColor.Inverse, {u"System / Caution", u"Badge", u"SystemFillColorCautionBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SystemFillColor.Critical, brushes.Text.FillColor.Inverse, {u"System / Critical", u"Badge", u"SystemFillColorCriticalBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SystemFillColor.SuccessBackground, nullptr, {u"System / Success Background", u"Infobar Background", u"SystemFillColorSuccessBackgroundBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SystemFillColor.CautionBackground, nullptr, {u"System / Caution Background", u"Infobar Background", u"SystemFillColorCautionBackgroundBrush", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SystemFillColor.CriticalBackground, nullptr, {u"System / Critical Background", u"Infobar Background", u"SystemFillColorCriticalBackgroundBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SystemFillColor.Attention, brushes.Text.FillColor.Inverse, {u"System / Attention", u"Badge", u"SystemFillColorAttentionBrush", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SystemFillColor.Neutral, brushes.Text.FillColor.Inverse, {u"System / Neutral", u"Badge", u"SystemFillColorNeutralBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SystemFillColor.Solid.Neutral, brushes.Text.FillColor.Inverse, {u"System / Solid Neutral", u"Neutral badges over content", u"SystemFillColorSolidNeutralBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SystemFillColor.AttentionBackground, nullptr, {u"System / Attention Background", u"Infobar Background", u"SystemFillColorAttentionBackgroundBrush", true, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(brushes.SystemFillColor.NeutralBackground, nullptr, {u"System / Neutral Background", u"Infobar Background", u"SystemFillColorNeutralBackgroundBrush", true, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(brushes.SystemFillColor.Solid.NeutralBackground, nullptr, {u"System / Solid Neutral Background", u"Neutral badges over content", u"SystemFillColorSolidNeutralBackgroundBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
        gallery::tileGrid(3, 1, {
            gallery::colorTile(brushes.SystemFillColor.Solid.AttentionBackground, nullptr, {u"System / Solid Attention Background", u"", u"SystemFillColorSolidAttentionBackgroundBrush", false, gallery::ColorBackdrop::None, false, 0, 2}),
        }),
    };
}

FrameworkElement highContrastSection() {
    return StackPanel {
        spacing = 4.0,
        TextBlock {textWrapping = TextWrapping::Wrap, u"Below are the default high contrast themes shown. The brush names are the same, and the OS will choose the right colors based on the selected theme."},
        TextBlock {Margin {0, 12, 0, 0}, styles.TextBlock.Subtitle, u"Aquatic"},
        gallery::tileGrid(4, 2, {
            gallery::colorTile(rgb(0xFF, 0xFF, 0xFF), rgb(0x20, 0x20, 0x20), {u"Window Text Color", u"Foreground / Text color for Headings, body copy, lists, placeholder text, app and window borders, any UI that can't be interacted with", u"SystemColorWindowTextColor", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(rgb(0x20, 0x20, 0x20), rgb(0xFF, 0xFF, 0xFF), {u"Window Color", u"Background of pages, panes, popups, and windows", u"SystemColorWindowColor", false, gallery::ColorBackdrop::None, false, 1, 0}),
            gallery::colorTile(rgb(0x26, 0x3B, 0x50), rgb(0x8E, 0xE3, 0xF0), {u"Highlight Text Color", u"Foreground color for text or UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightTextColor", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(rgb(0x8E, 0xE3, 0xF0), rgb(0x26, 0x3B, 0x50), {u"Highlight Color", u"Background or accent color for UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightColor", false, gallery::ColorBackdrop::None, false, 1, 1}),
            gallery::colorTile(rgb(0xFF, 0xFF, 0xFF), rgb(0x20, 0x20, 0x20), {u"Button Text Color", u"Foreground color for buttons and any UI that can be interacted with", u"SystemColorButtonTextColor", false, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(rgb(0x20, 0x20, 0x20), rgb(0xFF, 0xFF, 0xFF), {u"Button Face Color", u"Background color for buttons and any UI that can be interacted with", u"SystemColorButtonFaceColor", false, gallery::ColorBackdrop::None, false, 1, 2}),
            gallery::colorTile(rgb(0x75, 0xE9, 0xFC), rgb(0x20, 0x20, 0x20), {u"Hotlight Color", u"Foreground / Text color for hyperlink text", u"SystemColorHotlightColor", false, gallery::ColorBackdrop::None, false, 0, 3}),
            gallery::colorTile(rgb(0xA6, 0xA6, 0xA6), rgb(0xFF, 0xFF, 0xFF), {u"Gray Text Color / Disabled", u"Foreground / Text color for Inactive (disabled) UI", u"SystemColorGrayTextColor", false, gallery::ColorBackdrop::None, false, 1, 3}),
        }),
        TextBlock {Margin {0, 12, 0, 0}, styles.TextBlock.Subtitle, u"Desert"},
        gallery::tileGrid(4, 2, {
            gallery::colorTile(rgb(0x3D, 0x3D, 0x3D), rgb(0xFF, 0xFA, 0xEF), {u"Window Text Color", u"Foreground / Text color for Headings, body copy, lists, placeholder text, app and window borders, any UI that can't be interacted with", u"SystemColorWindowTextColor", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(rgb(0xFF, 0xFA, 0xEF), rgb(0x3D, 0x3D, 0x3D), {u"Window Color", u"Background of pages, panes, popups, and windows", u"SystemColorWindowColor", false, gallery::ColorBackdrop::None, false, 1, 0}),
            gallery::colorTile(rgb(0xFF, 0xF5, 0xE3), rgb(0x90, 0x39, 0x09), {u"Highlight Text Color", u"Foreground color for text or UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightTextColor", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(rgb(0x90, 0x39, 0x09), rgb(0xFF, 0xF5, 0xE3), {u"Highlight Color", u"Background or accent color for UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightColor", false, gallery::ColorBackdrop::None, false, 1, 1}),
            gallery::colorTile(rgb(0x20, 0x20, 0x20), rgb(0xFF, 0xFA, 0xEF), {u"Button Text Color", u"Foreground color for buttons and any UI that can be interacted with", u"SystemColorButtonTextColor", false, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(rgb(0xFF, 0xFA, 0xEF), rgb(0x20, 0x20, 0x20), {u"Button Face Color", u"Background color for buttons and any UI that can be interacted with", u"SystemColorButtonFaceColor", false, gallery::ColorBackdrop::None, false, 1, 2}),
            gallery::colorTile(rgb(0x1C, 0x5E, 0x75), rgb(0xFF, 0xFA, 0xEF), {u"Hotlight Color", u"Foreground / Text color for hyperlink text", u"SystemColorHotlightColor", false, gallery::ColorBackdrop::None, false, 0, 3}),
            gallery::colorTile(rgb(0x67, 0x67, 0x67), rgb(0xFF, 0xFA, 0xEF), {u"Gray Text Color / Disabled", u"Foreground / Text color for Inactive (disabled) UI", u"SystemColorGrayTextColor", false, gallery::ColorBackdrop::None, false, 1, 3}),
        }),
        TextBlock {Margin {0, 12, 0, 0}, styles.TextBlock.Subtitle, u"Dusk"},
        gallery::tileGrid(4, 2, {
            gallery::colorTile(rgb(0xFF, 0xFF, 0xFF), rgb(0x2D, 0x32, 0x36), {u"Window Text Color", u"Foreground / Text color for Headings, body copy, lists, placeholder text, app and window borders, any UI that can't be interacted with", u"SystemColorWindowTextColor", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(rgb(0x2D, 0x32, 0x36), rgb(0xFF, 0xFF, 0xFF), {u"Window Color", u"Background of pages, panes, popups, and windows", u"SystemColorWindowColor", false, gallery::ColorBackdrop::None, false, 1, 0}),
            gallery::colorTile(rgb(0x21, 0x2D, 0x3B), rgb(0xAB, 0xCF, 0xF2), {u"Highlight Text Color", u"Foreground color for text or UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightTextColor", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(rgb(0xAB, 0xCF, 0xF2), rgb(0x21, 0x2D, 0x3B), {u"Highlight Color", u"Background or accent color for UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightColor", false, gallery::ColorBackdrop::None, false, 1, 1}),
            gallery::colorTile(rgb(0xB6, 0xF6, 0xF0), rgb(0x2D, 0x32, 0x36), {u"Button Text Color", u"Foreground color for buttons and any UI that can be interacted with", u"SystemColorButtonTextColor", false, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(rgb(0x2D, 0x32, 0x36), rgb(0xB6, 0xF6, 0xF0), {u"Button Face Color", u"Background color for buttons and any UI that can be interacted with", u"SystemColorButtonFaceColor", false, gallery::ColorBackdrop::None, false, 1, 2}),
            gallery::colorTile(rgb(0x70, 0xEB, 0xDE), rgb(0x20, 0x20, 0x20), {u"Hotlight Color", u"Foreground / Text color for hyperlink text", u"SystemColorHotlightColor", false, gallery::ColorBackdrop::None, false, 0, 3}),
            gallery::colorTile(rgb(0xA6, 0xA6, 0xA6), rgb(0xFF, 0xFF, 0xFF), {u"Gray Text Color / Disabled", u"Foreground / Text color for Inactive (disabled) UI", u"SystemColorGrayTextColor", false, gallery::ColorBackdrop::None, false, 1, 3}),
        }),
        TextBlock {Margin {0, 12, 0, 0}, styles.TextBlock.Subtitle, u"Night Sky"},
        gallery::tileGrid(4, 2, {
            gallery::colorTile(rgb(0xFF, 0xFF, 0xFF), rgb(0x00, 0x00, 0x00), {u"Window Text Color", u"Foreground / Text color for Headings, body copy, lists, placeholder text, app and window borders, any UI that can't be interacted with", u"SystemColorWindowTextColor", false, gallery::ColorBackdrop::None, false, 0, 0}),
            gallery::colorTile(rgb(0x00, 0x00, 0x00), rgb(0xFF, 0xFF, 0xFF), {u"Window Color", u"Background of pages, panes, popups, and windows", u"SystemColorWindowColor", false, gallery::ColorBackdrop::None, false, 1, 0}),
            gallery::colorTile(rgb(0x2B, 0x2B, 0x2B), rgb(0xD6, 0xB4, 0xFD), {u"Highlight Text Color", u"Foreground color for text or UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightTextColor", false, gallery::ColorBackdrop::None, false, 0, 1}),
            gallery::colorTile(rgb(0xD6, 0xB4, 0xFD), rgb(0x2B, 0x2B, 0x2B), {u"Highlight Color", u"Background or accent color for UI that is selected, interacted with (hover, pressed), or in progress", u"SystemColorHighlightColor", false, gallery::ColorBackdrop::None, false, 1, 1}),
            gallery::colorTile(rgb(0xFF, 0xEE, 0x32), rgb(0x00, 0x00, 0x00), {u"Button Text Color", u"Foreground color for buttons and any UI that can be interacted with", u"SystemColorButtonTextColor", false, gallery::ColorBackdrop::None, false, 0, 2}),
            gallery::colorTile(rgb(0x00, 0x00, 0x00), rgb(0xFF, 0xEE, 0x32), {u"Button Face Color", u"Background color for buttons and any UI that can be interacted with", u"SystemColorButtonFaceColor", false, gallery::ColorBackdrop::None, false, 1, 2}),
            gallery::colorTile(rgb(0x80, 0x80, 0xFF), rgb(0xFF, 0xFF, 0xFF), {u"Hotlight Color", u"Foreground / Text color for hyperlink text", u"SystemColorHotlightColor", false, gallery::ColorBackdrop::None, false, 0, 3}),
            gallery::colorTile(rgb(0xA6, 0xA6, 0xA6), rgb(0x00, 0x00, 0x00), {u"Gray Text Color / Disabled", u"Foreground / Text color for Inactive (disabled) UI", u"SystemColorGrayTextColor", false, gallery::ColorBackdrop::None, false, 1, 3}),
        }),
    };
}

}  // namespace gallery
