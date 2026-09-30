// The option is one field of a model: the checkbox says "disable" over it and
// the control shows it as it is. The model lives while the page is on the
// screen.
struct Model {
    core::observable<bool> enabled{true};
};
auto const model = gallery::hold<Model>();
auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = RepeatButton {
    content = u"Click and hold",
    isEnabled = BindOutput {model->enabled},
    onClick = [output, clicks = std::make_shared<int>(0)] {
        output.text(L"Number of clicks: " + std::to_wstring(++*clicks));
    },
};

auto disable = CheckBox {
    content = u"Disable RepeatButton",
    isChecked = Bind {model->enabled, std::logical_not {}},
};