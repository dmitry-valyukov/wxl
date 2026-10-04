// The picker and the boxes around it share the fields of a model: a box writes its field and the picker shows it
// (BindOutput), the picker writes the color and the rectangle shows it (Bind and BindOutput).
struct Model {
    core::observable<Color> color {rgb(255, 255, 255)};
    core::observable<bool> moreButton {false}, colorSlider {true}, channelTextInput {true}, hexInput {true};
    core::observable<bool> alphaEnabled {false}, alphaSlider {true}, alphaTextInput {true};
    core::observable<int> shape {0};
};
auto const model = gallery::hold<Model>();

auto preview = Rectangle {
    height = 100,
    Margin {0, 12, 0, 0},
    stroke = brushes.Card.StrokeColorDefault,
    strokeThickness = 1.0,
    fill = BindOutput {model->color, [](Color color) { return SolidColorBrush {color = color}; }},
};

auto picker = ColorPicker {
    color = Bind {model->color},
    isMoreButtonVisible = BindOutput {model->moreButton},
    isColorSliderVisible = BindOutput {model->colorSlider},
    isColorChannelTextInputVisible = BindOutput {model->channelTextInput},
    isHexInputVisible = BindOutput {model->hexInput},
    isAlphaEnabled = BindOutput {model->alphaEnabled},
    isAlphaSliderVisible = BindOutput {model->alphaSlider},
    isAlphaTextInputVisible = BindOutput {model->alphaTextInput},
    colorSpectrumShape = BindOutput {model->shape, [](int index) { return index == 0 ? ColorSpectrumShape::Box : ColorSpectrumShape::Ring; }},
};

// A box that writes the field it is given.
auto const option = [](char16_t const* name, core::observable<bool>& field) {
    return CheckBox {content = name, isChecked = Bind {field}};
};

auto options = StackPanel {
    width = 250,
    Margin {0, -5, 0, 0},
    option(u"IsMoreButtonVisible", model->moreButton),
    option(u"IsColorSliderVisible", model->colorSlider),
    option(u"IsColorChannelTextInputVisible", model->channelTextInput),
    option(u"IsHexInputVisible", model->hexInput),
    option(u"Alpha Enabled", model->alphaEnabled),
    // The two boxes follow the alpha: they have no say while it is off.
    CheckBox {content = u"IsAlphaSliderVisible", isChecked = Bind {model->alphaSlider}, isEnabled = BindOutput {model->alphaEnabled}},
    CheckBox {content = u"IsAlphaTextInputVisible", isChecked = Bind {model->alphaTextInput}, isEnabled = BindOutput {model->alphaEnabled}},
    RadioButtons {
        header = u"Colorspectrum shape",
        selectedIndex = Bind {model->shape},
        RadioButton {content = u"Box"},
        RadioButton {content = u"Ring"},
    },
    StackPanel {
        Margin {0, 12, 0, 0},
        TextBlock {u"ColorPicker applied on a Rectangle"},
        preview,
    },
};
