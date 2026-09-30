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

auto output = TextBlock {};

auto button = Button {
    content = u"Standard button",
    isEnabled = BindOutput {model->enabled},
    onClick = [output] { output.text(u"You clicked: Button1"); },
};

auto disable = CheckBox {
    content = u"Disable button",
    isChecked = BindInput {model->disabled},
};