struct Model {
    core::observable<double> minimum {500.0}, maximum {1000.0}, step {10.0}, little {10.0}, value {800.0};
};
auto const model = gallery::hold<Model>();

auto output = TextBlock {text = BindOutput {model->value, [](double value) { return core::to_u16(value, std::chars_format::fixed, 0); }}};

auto slider = Slider {
    width = 200,
    Margin {0, 0, 10, 0},
    header = u"Control header",
    minimum = BindOutput {model->minimum},
    maximum = BindOutput {model->maximum},
    smallChange = BindOutput {model->little},
    stepFrequency = BindOutput {model->step},
    value = Bind {model->value},
};

const Preset field {
    spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Compact,
    Margin {10, 5, 0, 0},
    minWidth = 80,
};

auto options = Grid {
    rowDefinitions = u"*,*,*,*",
    columnDefinitions = u"auto,auto",
    TextBlock {u"Minimum:", row = 0, column = 0},
    NumberBox {
        field, row = 0, column = 1, value = Bind {model->minimum}, automationAccessibilityView = AccessibilityView::Raw, automationName = u"Minimum",
    },
    TextBlock {u"Maximum:", row = 1, column = 0, Margin {0, 7, 0, 0}},
    NumberBox {
        field, row = 1, column = 1, value = Bind {model->maximum}, automationAccessibilityView = AccessibilityView::Raw, automationName = u"Maximum",
    },
    TextBlock {u"StepFrequency:", row = 2, column = 0, Margin {0, 5, 0, 0}},
    NumberBox {
        field, row = 2, column = 1, minimum = 1.0, value = Bind {model->step}, automationAccessibilityView = AccessibilityView::Raw, automationName = u"Step Frequency",
    },
    TextBlock {u"SmallChange:", row = 3, column = 0, Margin {0, 5, 0, 0}},
    NumberBox {
        field, row = 3, column = 1, value = Bind {model->little}, automationAccessibilityView = AccessibilityView::Raw, automationName = u"Small Change",
    },
};
