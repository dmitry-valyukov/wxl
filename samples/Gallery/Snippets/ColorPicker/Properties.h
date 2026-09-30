auto preview = Rectangle {
    height = 100,
    Margin {0, 12, 0, 0},
    stroke = brushes.Card.StrokeColorDefault,
    strokeThickness = 1.0,
};

auto picker = ColorPicker {
    onColorChanged = [preview](ColorPicker const& self) {
        preview.fill(SolidColorBrush {color = self.color()});
    },
};

auto alphaSlider = CheckBox {
    content = u"IsAlphaSliderVisible",
    isChecked = true,
    isEnabled = false,
    onClick = [picker](CheckBox const& self) { picker.isAlphaSliderVisible(self.isChecked().value_or(false)); },
};

auto alphaTextInput = CheckBox {
    content = u"IsAlphaTextInputVisible",
    isChecked = true,
    isEnabled = false,
    onClick = [picker](CheckBox const& self) { picker.isAlphaTextInputVisible(self.isChecked().value_or(false)); },
};

auto options = StackPanel {
    width = 250,
    Margin {0, -5, 0, 0},
    CheckBox {
        content = u"IsMoreButtonVisible",
        isChecked = false,
        onClick = [picker](CheckBox const& self) { picker.isMoreButtonVisible(self.isChecked().value_or(false)); },
    },
    CheckBox {
        content = u"IsColorSliderVisible",
        isChecked = true,
        onClick = [picker](CheckBox const& self) { picker.isColorSliderVisible(self.isChecked().value_or(false)); },
    },
    CheckBox {
        content = u"IsColorChannelTextInputVisible",
        isChecked = true,
        onClick = [picker](CheckBox const& self) {
            picker.isColorChannelTextInputVisible(self.isChecked().value_or(false));
        },
    },
    CheckBox {
        content = u"IsHexInputVisible",
        isChecked = true,
        onClick = [picker](CheckBox const& self) { picker.isHexInputVisible(self.isChecked().value_or(false)); },
    },
    CheckBox {
        content = u"Alpha Enabled",
        onClick = [picker, alphaSlider, alphaTextInput](CheckBox const& self) {
            bool const on = self.isChecked().value_or(false);
            picker.isAlphaEnabled(on);
            alphaSlider.isEnabled(on);
            alphaTextInput.isEnabled(on);
        },
    },
    alphaSlider,
    alphaTextInput,
    RadioButtons {
        header = u"Colorspectrum shape",
        selectedIndex = 0,
        RadioButton {content = u"Box"},
        RadioButton {content = u"Ring"},
        onSelectionChanged = [picker](RadioButtons const& self) {
            picker.colorSpectrumShape(self.selectedIndex() == 0 ? ColorSpectrumShape::Box
                                                                : ColorSpectrumShape::Ring);
        },
    },
    StackPanel {
        Margin {0, 12, 0, 0},
        TextBlock {u"ColorPicker applied on a Rectangle"},
        preview,
    },
};