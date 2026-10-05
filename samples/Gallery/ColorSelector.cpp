// ColorSelector оригинала (Controls/ColorSelector): кнопка с образцом цвета, а в
// её выпадающей части — ColorPicker. Цвет — поле модели, оно же связано с
// пипеткой и образцом, поэтому страница читает и слушает его, а не контрол.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

FrameworkElement gallery::colorSelector(core::observable<Color>& model, char16_t const* name) {
    return SplitButton {
        Margin {0, 2, 0, 8},
        automationName = name,
        content = Border {
            width = 64,
            height = 32,
            Margin {-11, -6},
            CornerRadius {4, 0, 0, 4},
            background = BindOutput {model, [](Color value) { return SolidColorBrush {color = value}; }},
        },
        flyout = Flyout {
            placement = FlyoutPlacementMode::Bottom,
            content = ColorPicker {
                color = Bind {model},
                colorSpectrumShape = ColorSpectrumShape::Ring,
                isAlphaEnabled = false,
                isAlphaSliderVisible = true,
                isAlphaTextInputVisible = false,
                isColorChannelTextInputVisible = false,
                isColorSliderVisible = true,
                isHexInputVisible = false,
                isMoreButtonVisible = false,
            },
        },
    };
}
