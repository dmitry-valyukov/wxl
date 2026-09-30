// The option is a field of a model: the checkbox writes it, the button shows
// it. The model lives while the page is on the screen.
struct Model {
    core::observable<bool> disabled{false};
    core::observable<bool> enabled{true};

    Model() {
        enabled.follow(disabled, [](bool off) { return !off; });
    }
};
auto const model = gallery::hold(std::make_shared<Model>());

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
    isChecked = BindInput {model->disabled},
};